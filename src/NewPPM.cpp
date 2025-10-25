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
    knownNodes.reserve(1672385);
}

NewPPM::~NewPPM(){}

bool NewPPM::isValidContext(Dasher::CLanguageModel::Context context) const {
    return 0 <= context && context < knownContexts.size();
}

Dasher::CLanguageModel::Context NewPPM::CreateEmptyContext(){
    knownContexts.push_back(NewPPMContext({0,0}));
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
            auto child = FindChild(refContext.referencedNode, Symbol);
            if(child >= 0){
                refContext.order++;
                refContext.referencedNode = child;
                return;
            }
        }

        refContext.order--;
        refContext.referencedNode = knownNodes[refContext.referencedNode].vine;
    }

    if(refContext.referencedNode < 0) {
        refContext.referencedNode = 0;
        refContext.order = 0;
    }
}

NewPPMNode_Ptr NewPPM::AddSymbolToNode(NewPPMNode_Ptr Node, Dasher::symbol Symbol){
    //check if node already exists
    auto foundNode = FindChild(Node, Symbol);
    if(foundNode >= 0){
        knownNodes[foundNode].count++;

        if(!updateExclusions){
            // increase count moving up the vines to the root
            for (NewPPMNode_Ptr vine = knownNodes[foundNode].vine; vine > 0; vine=knownNodes[vine].vine) {
                knownNodes[vine].count++;
            }
        }
        return foundNode;
    }
    // check for capacity before creating new new element
    if(knownNodes.size() == knownNodes.capacity()) knownNodes.reserve(std::min(static_cast<size_t>(knownNodes.capacity()*3), maximumAmountOfNodes));
    knownNodes.emplace_back(); // create new node
    const NewPPMNode_Ptr newNode = knownNodes.size() - 1;
    knownNodes[newNode].sym = Symbol;

    auto& childArray = knownNodes[Node].children;
    // check for capacity before creating new new element
    if(childArray.get() == nullptr) childArray = std::make_unique<std::vector<NewPPMNode_Ptr>>();
    if(childArray->size() == childArray->capacity()) childArray->reserve(std::min(static_cast<int>(childArray->capacity()*1.5f+1), m_iNumSyms));
    childArray->emplace_back(newNode);
    knownNodes[newNode].vine = (Node == 0) ? 0 : AddSymbolToNode(knownNodes[Node].vine, Symbol);

    return newNode;
}

void NewPPM::LearnSymbol(Context Context, int Symbol){
    if(!isValidContext(Context)) return;

    auto& refContext = knownContexts[Context];

    refContext.referencedNode = AddSymbolToNode(refContext.referencedNode, Symbol);
    refContext.order++;

    // extended context too long? Shorten by one and follow the vine
    if(refContext.order > maxOrder){
        refContext.referencedNode = knownNodes[refContext.referencedNode].vine;
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

    for(NewPPMNode_Ptr curr = refContext.referencedNode; curr >= 0; curr=knownNodes[curr].vine){
        int Total = 0;
        
        // sum all child counts
        if(!knownNodes[curr].children) continue; // no children

        for(auto const& ref: *knownNodes[curr].children){
            Total += knownNodes[ref].count;
        }

        if(Total == 0) continue; // nothing to distribute between children, means 0 children as every child has at least count 1

        const unsigned int size_of_slice = ToSpend;

        for(auto const& ref: *knownNodes[curr].children){
            // optimized for decreased rounding error?
            const unsigned int p = static_cast<long long>(size_of_slice) * (100 * knownNodes[ref].count - beta) / (100 * Total + alpha);
            Probs[knownNodes[ref].sym] += p;
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