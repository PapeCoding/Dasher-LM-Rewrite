#include "DasherTypes.h"
#include "LanguageModel.h"
#include "SettingsStore.h"
#include <unordered_map>

typedef long long NewPPMNode_Ptr; //references into our vector of known nodes

struct NewPPMNode {
    Dasher::symbol sym;
    unsigned count = 1;
    NewPPMNode_Ptr vine = -1; // reference up the tree, where the referenced node is basically the same prefix but without the last character
    std::vector<NewPPMNode_Ptr> children;
};

struct NewPPMContext {
    unsigned order = 0;
    NewPPMNode_Ptr referencedNode = -1;
};

class NewPPM : public Dasher::CLanguageModel
{
    public:
        NewPPM(Dasher::CSettingsStore* pSettingsStore, int iNumSyms);
        ~NewPPM();

        virtual Context CreateEmptyContext();
        virtual Context CloneContext(Context Context);
        virtual void ReleaseContext(Context Context);

        virtual void EnterSymbol(Context context, int Symbol);
        virtual void LearnSymbol(Context context, int Symbol);

        virtual void GetProbs(Context Context, std::vector<unsigned int>& Probs, int iNorm, int iUniform) const;

    private:
        inline bool isValidContext(Context Context) const;
        NewPPMNode_Ptr AddSymbolToNode(NewPPMNode_Ptr Node, Dasher::symbol Symbol);

        const NewPPMNode_Ptr FindChild(const NewPPMNode_Ptr& Node, const Dasher::symbol& Symbol){
            for(const auto& ref : knownNodes[Node].children){
                if(knownNodes[ref].sym == Symbol) return ref;
            }
            return -1;
        }

        std::vector<NewPPMNode> knownNodes; //[0] is root node
        std::vector<NewPPMContext> knownContexts;

        bool updateExclusions = false;
        unsigned int maxOrder = 5;
        size_t maximumAmountOfNodes = 0;
        Dasher::CSettingsStore* settings;
};