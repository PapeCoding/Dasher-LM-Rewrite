#include "NewPPM.h"
#include "Parameters.h"

NewPPM::NewPPM(Dasher::CSettingsStore* pSettingsStore, int iNumSyms) : 
    CLanguageModel(iNumSyms),
    updateExclusions(pSettingsStore->GetLongParameter(Dasher::Parameter::LP_LM_UPDATE_EXCLUSION) != 0),
    settings(pSettingsStore),
    knownNodes(1000)
{
    rootNode = knownNodes.Alloc();
    rootNode->sym = -1;
}

NewPPM::~NewPPM(){}

bool NewPPM::isValidContext(Dasher::CLanguageModel::Context context) const {
    return 0 <= context && context < knownContexts.size();
}

Dasher::CLanguageModel::Context NewPPM::CreateEmptyContext(){
    knownContexts.push_back(NewPPMContext({0, rootNode}));
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
    while(refContext.referencedNode){
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

    if(!refContext.referencedNode) {
        refContext.referencedNode = 0;
        refContext.order = 0;
    }
}

NewPPMNode* NewPPM::AddSymbolToNode(NewPPMNode* Node, Dasher::symbol Symbol){
    //check if node already exists
    NewPPMNode* foundNode = FindChild(Node, Symbol);
    if(foundNode){
        foundNode->count++;

        if(!updateExclusions){
            // increase count moving up the vines to the root
            for (NewPPMNode* vine = foundNode->vine; vine; vine=vine->vine) {
                vine->count++;
            }
        }
        return foundNode;
    }
    NewPPMNode* newNode = knownNodes.Alloc(); // create new node
    newNode->sym = Symbol;
    Node->children.Add(newNode);
    newNode->vine = (Node == rootNode) ? Node : AddSymbolToNode(Node->vine, Symbol);

    return newNode;
}

NewPPMNode* NewPPM::FindChild(const NewPPMNode* Node, const Dasher::symbol& Symbol) {
    for(NewPPMNode* n : Node->children) {
        if(n->sym == Symbol) return n;
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
        int Total = 0;

        for(NewPPMNode* child : curr->children){
            Total += child->count;
        }

        if(Total == 0) continue; // nothing to distribute between children, means 0 children as every child has at least count 1

        const unsigned int size_of_slice = ToSpend;
        for(NewPPMNode* child : curr->children){
            const unsigned int p = static_cast<long long>(size_of_slice) * (100 * child->count - beta) / (100 * Total + alpha);
            Probs[child->sym] += p;
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