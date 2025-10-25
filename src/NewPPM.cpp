#include "NewPPM.h"
#include "Parameters.h"
#include <cmath>

NewPPM::NewPPM(Dasher::CSettingsStore* pSettingsStore, int iNumSyms) : 
    CLanguageModel(iNumSyms),
    updateExclusions(pSettingsStore->GetLongParameter(Dasher::Parameter::LP_LM_UPDATE_EXCLUSION) != 0),
    settings(pSettingsStore)
{
    // compute maximumAmountOfNodes = m_iNumSyms^(maxOrder+1) - 1 manually, as there is no `int pow(int, int)` currently
    maximumAmountOfNodes = m_iNumSyms;
    for(unsigned int i = 1; i <= maxOrder; i++)  maximumAmountOfNodes *= static_cast<unsigned int>(m_iNumSyms);
    maximumAmountOfNodes--;

    knownNodes.push_back(NewPPMNode({-1})); // Root Node
    knownNodes.reserve(1672383);
}

NewPPM::~NewPPM(){}

bool NewPPM::isValidContext(Dasher::CLanguageModel::Context context) const {
    return 0 <= context && context < knownContexts.size();
}

Dasher::CLanguageModel::Context NewPPM::CreateEmptyContext(){
    knownContexts.push_back(NewPPMContext({0, &knownNodes[0]}));
    return knownContexts.size() - 1;
}

Dasher::CLanguageModel::Context NewPPM::CloneContext(Context Context){
    if(!isValidContext(Context)) return -1;

    const Dasher::CLanguageModel::Context newContext = CreateEmptyContext();
    knownContexts[newContext] = knownContexts[Context];
    return newContext;
}

void NewPPM::ReleaseContext(Context Context){
    if(!isValidContext(Context)) return;
    knownContexts.erase(knownContexts.begin() + Context);
}

void NewPPM::EnterSymbol(Context Context, int Symbol){
    if(!isValidContext(Context)) return;

    auto& refContext = knownContexts[Context];
    while(refContext.referencedNode >= 0){
        // only extend the context if it is not too long already
        if(refContext.order < maxOrder){
            NewPPMNode* child = FindChild(refContext.referencedNode, Symbol);
            if(child){
                refContext.order++;
                refContext.referencedNode = child;
                return;
            }
        }

        refContext.order--;
        refContext.referencedNode = refContext.referencedNode->vine;
    }

    if(refContext.referencedNode < 0) {
        refContext.referencedNode = 0;
        refContext.order = 0;
    }
}

NewPPMNode* NewPPM::AddSymbolToNode(NewPPMNode* Node, Dasher::symbol Symbol){
    //check if node already exists
    NewPPMNode* foundNode = FindChild(Node, Symbol);
    if(foundNode){
        foundNode->count++;
        (*foundNode->parentCounter)++;

        if(!updateExclusions){
            // increase count moving up the vines to the root
            for (NewPPMNode* vine = foundNode->vine; vine; vine=vine->vine) {
                vine->count++;
                (*vine->parentCounter)++;
            }
        }
        return foundNode;
    }
    // check for capacity before creating new new element
    if(knownNodes.size() == knownNodes.capacity()) knownNodes.reserve(std::min(static_cast<size_t>(knownNodes.capacity()*3), maximumAmountOfNodes));
    NewPPMNode* newNode = &knownNodes.emplace_back(); // create new node
    newNode->sym = Symbol;
    newNode->parentCounter = &Node->childSum;

    // make us the new head of the child list
    newNode->next_sibling = Node->first_child; // if it is empty this is just a nullptr assignment
    Node->first_child = newNode;

    newNode->vine = (Node == knownNodes.data()) ? Node : AddSymbolToNode(Node->vine, Symbol);

    return newNode;
}

NewPPMNode* NewPPM::FindChild(const NewPPMNode* Node, const Dasher::symbol& Symbol) {
    if (!Node->first_child) return nullptr;
    for (NewPPMNode* ref = Node->first_child; ref; ref = ref->next_sibling) {
        if(ref->sym == Symbol) return ref;
    }
    return nullptr;
}

void NewPPM::LearnSymbol(Context Context, int Symbol){
    if(!isValidContext(Context)) return;

    auto& refContext = knownContexts[Context];

    refContext.referencedNode = AddSymbolToNode(refContext.referencedNode, Symbol);
    refContext.order++;

    // extended context too long? Shorten by one and follow the vine
    if(refContext.order > maxOrder){
        refContext.referencedNode = refContext.referencedNode->vine;
        refContext.order--;
    }
}

void NewPPM::GetProbs(Context Context, std::vector<unsigned int>& Probs, int iNorm, int iUniform) const{
    if(!isValidContext(Context)) return;

    auto& refContext = knownContexts[Context];

    unsigned int ToSpend = iNorm;
    unsigned int UniformLeft = iUniform;

    Probs.assign(m_iNumSyms + 1, UniformLeft / m_iNumSyms);
    Probs[0] = 0;
    ToSpend -= static_cast<unsigned int>(UniformLeft / m_iNumSyms) * m_iNumSyms; //compensates for rounding

    const int alpha = settings->GetLongParameter(Dasher::Parameter::LP_LM_ALPHA);
    const int beta = settings->GetLongParameter(Dasher::Parameter::LP_LM_BETA);

    for(NewPPMNode* curr = refContext.referencedNode; curr; curr=curr->vine){
        const int Total = curr->childSum;

        const unsigned int size_of_slice = ToSpend;

        for(NewPPMNode* ref = curr->first_child; ref; ref = ref->next_sibling){
            // optimized for decreased rounding error?
            const unsigned int p = static_cast<long long>(size_of_slice) * (100 * ref->count - beta) / (100 * Total + alpha);
            Probs[ref->sym] += p;
            ToSpend -= p;
        }
    }
    
    //uniformly distribute among all probs (except [0])
    for(unsigned int i = 1; i < Probs.size(); i++) {
        const unsigned int p = ToSpend / (static_cast<unsigned int>(Probs.size()) - i);
        Probs[i] += p;
        ToSpend -= p;
    }
}