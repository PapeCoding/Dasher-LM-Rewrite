#pragma once

#include "DasherTypes.h"
#include "LanguageModel.h"
#include "LazyArray.h"
#include "SettingsStore.h"
#include "SimplePooledAlloc.h"

struct NewPPMNode {
    Dasher::symbol sym;
    unsigned count = 1;
    NewPPMNode* vine = nullptr; // reference up the tree, where the referenced node is basically the same prefix but without the last character
    
    unsigned childCount = 0;
    union {
        NewPPMNode** array;
        NewPPMNode* firstElement = nullptr;
    };
};

struct NewPPMContext {
    unsigned order = 0;
    NewPPMNode* referencedNode = nullptr;
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
        bool isValidContext(Context Context) const;
        NewPPMNode* AddSymbolToNode(NewPPMNode* Node, Dasher::symbol Symbol);

        NewPPMNode* FindChild(const NewPPMNode* Node, const Dasher::symbol& Symbol);

        CSimplePooledAlloc<NewPPMNode> knownNodes;
        NewPPMNode* rootNode;
        std::vector<NewPPMContext> knownContexts;

        bool updateExclusions = false;
        unsigned int maxOrder = 5;
        Dasher::CSettingsStore* settings;
};