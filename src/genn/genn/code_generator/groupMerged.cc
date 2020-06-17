#include "code_generator/groupMerged.h"

// PLOG includes
#include <plog/Log.h>

// GeNN includes
#include "modelSpecInternal.h"

// GeNN code generator includes
#include "code_generator/backendBase.h"
#include "code_generator/codeGenUtils.h"
#include "code_generator/codeStream.h"
#include "code_generator/mergedStructGenerator.h"

//----------------------------------------------------------------------------
// CodeGenerator::NeuronSpikeQueueUpdateGroupMerged
//----------------------------------------------------------------------------
const std::string CodeGenerator::NeuronSpikeQueueUpdateGroupMerged::name = "NeuronSpikeQueueUpdate";
//----------------------------------------------------------------------------
CodeGenerator::NeuronSpikeQueueUpdateGroupMerged::NeuronSpikeQueueUpdateGroupMerged(size_t index, const std::string &precision, const std::string &timePrecision, const BackendBase &backend,
                                                                                    const std::vector<std::reference_wrapper<const NeuronGroupInternal>> &groups)
:   GroupMerged<NeuronGroupInternal, NeuronSpikeQueueUpdateGroupMerged>(index, precision, groups)
{
    if(getArchetype().isDelayRequired()) {
        m_Gen.addField("unsigned int", "numDelaySlots",
                     [](const NeuronGroupInternal &ng, size_t) { return std::to_string(ng.getNumDelaySlots()); });

        m_Gen.addField("volatile unsigned int*", "spkQuePtr",
                      [&backend](const NeuronGroupInternal &ng, size_t)
                      {
                          return "getSymbolAddress(" + backend.getScalarPrefix() + "spkQuePtr" + ng.getName() + ")";
                      });
    } 

    m_Gen.addPointerField("unsigned int", "spkCnt", backend.getArrayPrefix() + "glbSpkCnt");

    if(getArchetype().isSpikeEventRequired()) {
        m_Gen.addPointerField("unsigned int", "spkCntEvnt", backend.getArrayPrefix() + "glbSpkCntEvnt");
    }
}
//----------------------------------------------------------------------------
void CodeGenerator::NeuronSpikeQueueUpdateGroupMerged::generate(const BackendBase &backend, CodeStream &definitionsInternal,
                                                                CodeStream &definitionsInternalFunc, CodeStream &definitionsInternalVar,
                                                                CodeStream &runnerVarDecl, CodeStream &runnerMergedStructAlloc,
                                                                MergedStructData &mergedStructData, const std::string &precision) const
{
    // Generate structure definitions and instantiation
    m_Gen.generate(backend, definitionsInternal, definitionsInternalFunc, definitionsInternalVar, runnerVarDecl, runnerMergedStructAlloc,
                   mergedStructData, NeuronSpikeQueueUpdateGroupMerged::name);
}
//----------------------------------------------------------------------------
void CodeGenerator::NeuronSpikeQueueUpdateGroupMerged::genMergedGroupSpikeCountReset(CodeStream &os) const
{
    if(getArchetype().isDelayRequired()) { // with delay
        if(getArchetype().isSpikeEventRequired()) {
            os << "group->spkCntEvnt[*group->spkQuePtr] = 0;" << std::endl;
        }
        if(getArchetype().isTrueSpikeRequired()) {
            os << "group->spkCnt[*group->spkQuePtr] = 0;" << std::endl;
        }
        else {
            os << "group->spkCnt[0] = 0;" << std::endl;
        }
    }
    else { // no delay
        if(getArchetype().isSpikeEventRequired()) {
            os << "group->spkCntEvnt[0] = 0;" << std::endl;
        }
        os << "group->spkCnt[0] = 0;" << std::endl;
    }
}

//----------------------------------------------------------------------------
// CodeGenerator::NeuronGroupMergedBase
//----------------------------------------------------------------------------
bool CodeGenerator::NeuronGroupMergedBase::isParamHeterogeneous(size_t index) const
{
    return isParamValueHeterogeneous(index, [](const NeuronGroupInternal &ng) { return ng.getParams(); });
}
//----------------------------------------------------------------------------
bool CodeGenerator::NeuronGroupMergedBase::isDerivedParamHeterogeneous(size_t index) const
{
    return isParamValueHeterogeneous(index, [](const NeuronGroupInternal &ng) { return ng.getDerivedParams(); });
}
//----------------------------------------------------------------------------
bool CodeGenerator::NeuronGroupMergedBase::isVarInitParamHeterogeneous(size_t varIndex, size_t paramIndex) const
{
    // If parameter isn't referenced in code, there's no point implementing it hetereogeneously!
    const auto *varInitSnippet = getArchetype().getVarInitialisers().at(varIndex).getSnippet();
    const std::string paramName = varInitSnippet->getParamNames().at(paramIndex);
    return isParamValueHeterogeneous({varInitSnippet->getCode()}, paramName, paramIndex,
                                     [varIndex](const NeuronGroupInternal &sg)
                                     {
                                         return sg.getVarInitialisers().at(varIndex).getParams();
                                     });
}
//----------------------------------------------------------------------------
bool CodeGenerator::NeuronGroupMergedBase::isVarInitDerivedParamHeterogeneous(size_t varIndex, size_t paramIndex) const
{
    // If parameter isn't referenced in code, there's no point implementing it hetereogeneously!
    const auto *varInitSnippet = getArchetype().getVarInitialisers().at(varIndex).getSnippet();
    const std::string derivedParamName = varInitSnippet->getDerivedParams().at(paramIndex).name;
    return isParamValueHeterogeneous({varInitSnippet->getCode()}, derivedParamName, paramIndex,
                                     [varIndex](const NeuronGroupInternal &sg)
                                     {
                                         return sg.getVarInitialisers().at(varIndex).getDerivedParams();
                                     });
}
//----------------------------------------------------------------------------
bool CodeGenerator::NeuronGroupMergedBase::isCurrentSourceParamHeterogeneous(size_t childIndex, size_t paramIndex) const
{
    // If parameter isn't referenced in code, there's no point implementing it hetereogeneously!
    const auto *csm = getArchetype().getCurrentSources().at(childIndex)->getCurrentSourceModel();
    const std::string paramName = csm->getParamNames().at(paramIndex);
    return isChildParamValueHeterogeneous({csm->getInjectionCode()}, paramName, childIndex, paramIndex, m_SortedCurrentSources,
                                          [](const CurrentSourceInternal *cs) { return cs->getParams(); });
}
//----------------------------------------------------------------------------
bool CodeGenerator::NeuronGroupMergedBase::isCurrentSourceDerivedParamHeterogeneous(size_t childIndex, size_t paramIndex) const
{
    // If derived parameter isn't referenced in code, there's no point implementing it hetereogeneously!
    const auto *csm = getArchetype().getCurrentSources().at(childIndex)->getCurrentSourceModel();
    const std::string derivedParamName = csm->getDerivedParams().at(paramIndex).name;
    return isChildParamValueHeterogeneous({csm->getInjectionCode()}, derivedParamName, childIndex, paramIndex, m_SortedCurrentSources,
                                          [](const CurrentSourceInternal *cs) { return cs->getDerivedParams(); });
 
}
//----------------------------------------------------------------------------
bool CodeGenerator::NeuronGroupMergedBase::isCurrentSourceVarInitParamHeterogeneous(size_t childIndex, size_t varIndex, size_t paramIndex) const
{
    const auto *varInitSnippet = getArchetype().getCurrentSources().at(childIndex)->getVarInitialisers().at(varIndex).getSnippet();
    const std::string paramName = varInitSnippet->getParamNames().at(paramIndex);
    return isChildParamValueHeterogeneous({varInitSnippet->getCode()}, paramName, childIndex, paramIndex, m_SortedCurrentSources,
                                          [varIndex](const CurrentSourceInternal *cs) { return cs->getVarInitialisers().at(varIndex).getParams(); });
}
//----------------------------------------------------------------------------
bool CodeGenerator::NeuronGroupMergedBase::isCurrentSourceVarInitDerivedParamHeterogeneous(size_t childIndex, size_t varIndex, size_t paramIndex) const
{
    const auto *varInitSnippet = getArchetype().getCurrentSources().at(childIndex)->getVarInitialisers().at(varIndex).getSnippet();
    const std::string derivedParamName = varInitSnippet->getDerivedParams().at(paramIndex).name;
    return isChildParamValueHeterogeneous({varInitSnippet->getCode()}, derivedParamName, childIndex, paramIndex, m_SortedCurrentSources,
                                          [varIndex](const CurrentSourceInternal *cs) { return cs->getVarInitialisers().at(varIndex).getDerivedParams(); });
}
//----------------------------------------------------------------------------
bool CodeGenerator::NeuronGroupMergedBase::isPSMParamHeterogeneous(size_t childIndex, size_t paramIndex) const
{  
    // If parameter isn't referenced in code, there's no point implementing it hetereogeneously!
    const auto *psm = getArchetype().getMergedInSyn().at(childIndex).first->getPSModel();
    const std::string paramName = psm->getParamNames().at(paramIndex);
    return isChildParamValueHeterogeneous({psm->getApplyInputCode(), psm->getDecayCode()}, paramName, childIndex, paramIndex, m_SortedMergedInSyns,
                                          [](const std::pair<SynapseGroupInternal *, std::vector<SynapseGroupInternal *>> &inSyn)
                                          {
                                              return inSyn.first->getPSParams();
                                          });
}
//----------------------------------------------------------------------------
bool CodeGenerator::NeuronGroupMergedBase::isPSMDerivedParamHeterogeneous(size_t childIndex, size_t paramIndex) const
{
    // If parameter isn't referenced in code, there's no point implementing it hetereogeneously!
    const auto *psm = getArchetype().getMergedInSyn().at(childIndex).first->getPSModel();
    const std::string derivedParamName = psm->getDerivedParams().at(paramIndex).name;
    return isChildParamValueHeterogeneous({psm->getApplyInputCode(), psm->getDecayCode()}, derivedParamName, childIndex, paramIndex, m_SortedMergedInSyns,
                                          [](const std::pair<SynapseGroupInternal *, std::vector<SynapseGroupInternal *>> &inSyn)
                                          {
                                              return inSyn.first->getPSDerivedParams();
                                          });
}
//----------------------------------------------------------------------------
bool CodeGenerator::NeuronGroupMergedBase::isPSMGlobalVarHeterogeneous(size_t childIndex, size_t varIndex) const
{
    // If synapse group doesn't have individual PSM variables to start with, return false
    const auto *sg = getArchetype().getMergedInSyn().at(childIndex).first;
    if(sg->getMatrixType() & SynapseMatrixWeight::INDIVIDUAL_PSM) {
        return false;
    }
    else {
        const auto *psm = getArchetype().getMergedInSyn().at(childIndex).first->getPSModel();
        const std::string varName = psm->getVars().at(varIndex).name;
        return isChildParamValueHeterogeneous({psm->getApplyInputCode(), psm->getDecayCode()}, varName, childIndex, varIndex, m_SortedMergedInSyns,
                                              [](const std::pair<SynapseGroupInternal *, std::vector<SynapseGroupInternal *>> &inSyn)
                                              {
                                                  return inSyn.first->getPSConstInitVals();
                                              });
    }
}
//----------------------------------------------------------------------------
bool CodeGenerator::NeuronGroupMergedBase::isPSMVarInitParamHeterogeneous(size_t childIndex, size_t varIndex, size_t paramIndex) const
{
    const auto *varInitSnippet = getArchetype().getMergedInSyn().at(childIndex).first->getPSVarInitialisers().at(varIndex).getSnippet();
    const std::string paramName = varInitSnippet->getParamNames().at(paramIndex);
    return isChildParamValueHeterogeneous({varInitSnippet->getCode()}, paramName, childIndex, paramIndex, m_SortedMergedInSyns,
                                          [varIndex](const std::pair<SynapseGroupInternal *, std::vector<SynapseGroupInternal *>> &inSyn) 
                                          { 
                                              return inSyn.first->getPSVarInitialisers().at(varIndex).getParams();
                                          });
}
//----------------------------------------------------------------------------
bool CodeGenerator::NeuronGroupMergedBase::isPSMVarInitDerivedParamHeterogeneous(size_t childIndex, size_t varIndex, size_t paramIndex) const
{
    const auto *varInitSnippet = getArchetype().getMergedInSyn().at(childIndex).first->getPSVarInitialisers().at(varIndex).getSnippet();
    const std::string derivedParamName = varInitSnippet->getDerivedParams().at(paramIndex).name;
    return isChildParamValueHeterogeneous({varInitSnippet->getCode()}, derivedParamName, childIndex, paramIndex, m_SortedMergedInSyns,
                                          [varIndex](const std::pair<SynapseGroupInternal *, std::vector<SynapseGroupInternal *>> &inSyn) 
                                          { 
                                              return inSyn.first->getPSVarInitialisers().at(varIndex).getDerivedParams();
                                          });
}
//----------------------------------------------------------------------------
CodeGenerator::NeuronGroupMergedBase::NeuronGroupMergedBase(size_t index, const std::string &precision, const std::string &timePrecision, const BackendBase &backend, 
                                                            bool init, const std::vector<std::reference_wrapper<const NeuronGroupInternal>> &groups)
:   CodeGenerator::GroupMerged<NeuronGroupInternal, NeuronGroupMergedBase>(index, precision, groups)
{
    // Build vector of vectors containing each child group's merged in syns, ordered to match those of the archetype group
    orderNeuronGroupChildren(m_SortedMergedInSyns, &NeuronGroupInternal::getMergedInSyn,
                             [init](const std::pair<SynapseGroupInternal *, std::vector<SynapseGroupInternal *>> &a,
                                    const std::pair<SynapseGroupInternal *, std::vector<SynapseGroupInternal *>> &b)
                             {
                                 return init ? a.first->canPSInitBeMerged(*b.first) : a.first->canPSBeMerged(*b.first);
                             });

    // Build vector of vectors containing each child group's current sources, ordered to match those of the archetype group
    orderNeuronGroupChildren(m_SortedCurrentSources, &NeuronGroupInternal::getCurrentSources,
                             [init](const CurrentSourceInternal *a, const CurrentSourceInternal *b)
                             {
                                 return init ? a->canInitBeMerged(*b) : a->canBeMerged(*b);
                             });

    m_Gen.addField("unsigned int", "numNeurons",
                   [](const NeuronGroupInternal &ng, size_t) { return std::to_string(ng.getNumNeurons()); });

    m_Gen.addPointerField("unsigned int", "spkCnt", backend.getArrayPrefix() + "glbSpkCnt");
    m_Gen.addPointerField("unsigned int", "spk", backend.getArrayPrefix() + "glbSpk");

    if(getArchetype().isSpikeEventRequired()) {
        m_Gen.addPointerField("unsigned int", "spkCntEvnt", backend.getArrayPrefix() + "glbSpkCntEvnt");
        m_Gen.addPointerField("unsigned int", "spkEvnt", backend.getArrayPrefix() + "glbSpkEvnt");
    }

    if(getArchetype().isDelayRequired()) {
        m_Gen.addField("volatile unsigned int*", "spkQuePtr",
                     [&backend](const NeuronGroupInternal &ng, size_t)
                     {
                         return "getSymbolAddress(" + backend.getScalarPrefix() + "spkQuePtr" + ng.getName() + ")";
                     });
    }

    if(getArchetype().isSpikeTimeRequired()) {
        m_Gen.addPointerField(timePrecision, "sT", backend.getArrayPrefix() + "sT");
    }

    if(backend.isPopulationRNGRequired() && getArchetype().isSimRNGRequired()) {
        m_Gen.addPointerField("curandState", "rng", backend.getArrayPrefix() + "rng");
    }

    // Loop through variables
    const NeuronModels::Base *nm = getArchetype().getNeuronModel();
    const auto vars = nm->getVars();
    const auto &varInit = getArchetype().getVarInitialisers();
    assert(vars.size() == varInit.size());
    for(size_t v = 0; v < vars.size(); v++) {
        // If we're not initialising or if there is initialization code for this variable
        const auto var = vars[v];
        if(!init || !varInit[v].getSnippet()->getCode().empty()) {
            m_Gen.addPointerField(var.type, var.name, backend.getArrayPrefix() + var.name);
        }

        // If we're initializing, add any var init EGPs to structure
        if(init) {
            m_Gen.addEGPs(varInit[v].getSnippet()->getExtraGlobalParams(), backend.getArrayPrefix(), var.name);
        }
    }

    // If we're generating a struct for initialization
    if(init) {
        // Add heterogeneous var init parameters
        m_Gen.addHeterogeneousVarInitParams(vars, &NeuronGroupInternal::getVarInitialisers,
                                            &NeuronGroupMergedBase::isVarInitParamHeterogeneous);

        m_Gen.addHeterogeneousVarInitDerivedParams(vars, &NeuronGroupInternal::getVarInitialisers,
                                                   &NeuronGroupMergedBase::isVarInitDerivedParamHeterogeneous);
    }
    // Otherwise
    else {
        m_Gen.addEGPs(nm->getExtraGlobalParams(), backend.getArrayPrefix());

        // Add heterogeneous neuron model parameters
        m_Gen.addHeterogeneousParams(getArchetype().getNeuronModel()->getParamNames(),
                                     [](const NeuronGroupInternal &ng) { return ng.getParams(); },
                                   &NeuronGroupMergedBase::isParamHeterogeneous);

        // Add heterogeneous neuron model derived parameters
        m_Gen.addHeterogeneousDerivedParams(getArchetype().getNeuronModel()->getDerivedParams(),
                                            [](const NeuronGroupInternal &ng) { return ng.getDerivedParams(); },
                                            &NeuronGroupMergedBase::isDerivedParamHeterogeneous);
    }

    // Loop through merged synaptic inputs in archetypical neuron group
    for(size_t i = 0; i < getArchetype().getMergedInSyn().size(); i++) {
        const SynapseGroupInternal *sg = getArchetype().getMergedInSyn()[i].first;

        // Add pointer to insyn
        addMergedInSynPointerField(precision, "inSynInSyn", i, backend.getArrayPrefix() + "inSyn");

        // Add pointer to dendritic delay buffer if required
        if(sg->isDendriticDelayRequired()) {
            addMergedInSynPointerField(precision, "denDelayInSyn", i, backend.getArrayPrefix() + "denDelay");

            m_Gen.addField("volatile unsigned int*", "denDelayPtrInSyn" + std::to_string(i),
                           [&backend, i, this](const NeuronGroupInternal &, size_t groupIndex)
                           {
                               const std::string &targetName = m_SortedMergedInSyns.at(groupIndex).at(i).first->getPSModelTargetName();
                               return "getSymbolAddress(" + backend.getScalarPrefix() + "denDelayPtr" + targetName + ")";
                           });
        }

        // Loop through variables
        const auto vars = sg->getPSModel()->getVars();
        const auto &varInit = sg->getPSVarInitialisers();
        for(size_t v = 0; v < vars.size(); v++) {
            // If PSM has individual variables
            const auto var = vars[v];
            if(sg->getMatrixType() & SynapseMatrixWeight::INDIVIDUAL_PSM) {
                // Add pointers to state variable
                if(!init || !varInit[v].getSnippet()->getCode().empty()) {
                    addMergedInSynPointerField(var.type, var.name + "InSyn", i, backend.getArrayPrefix() + var.name);
                }

                // If we're generating an initialization structure, also add any heterogeneous parameters, derived parameters or extra global parameters required for initializers
                if(init) {
                    const auto *varInitSnippet = varInit.at(v).getSnippet();
                    auto getVarInitialiserFn = [this](size_t groupIndex, size_t childIndex)
                                               {
                                                   return m_SortedMergedInSyns.at(groupIndex).at(childIndex).first->getPSVarInitialisers();
                                               };
                    addHeterogeneousChildVarInitParams(varInitSnippet->getParamNames(), i, v, var.name + "InSyn",
                                                       &NeuronGroupMergedBase::isPSMVarInitParamHeterogeneous, getVarInitialiserFn);
                    addHeterogeneousChildVarInitDerivedParams(varInitSnippet->getDerivedParams(), i, v, var.name + "InSyn",
                                                              &NeuronGroupMergedBase::isPSMVarInitDerivedParamHeterogeneous, getVarInitialiserFn);
                    addChildEGPs(varInitSnippet->getExtraGlobalParams(), i, backend.getArrayPrefix(), var.name + "InSyn",
                                 [var, this](size_t groupIndex, size_t childIndex)
                                 {
                                     return var.name + m_SortedMergedInSyns.at(groupIndex).at(childIndex).first->getPSModelTargetName();
                                 });
                }
            }
            // Otherwise, if postsynaptic model variables are global and we're updating 
            // **NOTE** global variable values aren't useful during initialization
            else if(!init) {
                // If GLOBALG variable should be implemented heterogeneously, add value
                if(isPSMGlobalVarHeterogeneous(i, v)) {
                    m_Gen.addScalarField(var.name + "InSyn" + std::to_string(i),
                                         [this, i, v](const NeuronGroupInternal &, size_t groupIndex)
                                         {
                                             const double val = m_SortedMergedInSyns.at(groupIndex).at(i).first->getPSConstInitVals().at(v);
                                             return Utils::writePreciseString(val);
                                         });
                }
            }
        }

        if(!init) {
            // Add any heterogeneous postsynaptic model parameters
            const auto paramNames = sg->getPSModel()->getParamNames();
            addHeterogeneousChildParams(paramNames, i, "InSyn", &NeuronGroupMergedBase::isPSMParamHeterogeneous,
                                        [this](size_t groupIndex, size_t childIndex, size_t paramIndex)
                                        {
                                            return m_SortedMergedInSyns.at(groupIndex).at(childIndex).first->getPSParams().at(paramIndex);
                                        });

            // Add any heterogeneous postsynaptic mode derived parameters
            const auto derivedParams = sg->getPSModel()->getDerivedParams();
            addHeterogeneousChildDerivedParams(derivedParams, i, "InSyn", &NeuronGroupMergedBase::isPSMDerivedParamHeterogeneous,
                                               [this](size_t groupIndex, size_t childIndex, size_t paramIndex)
                                               {
                                                    return m_SortedMergedInSyns.at(groupIndex).at(childIndex).first->getPSDerivedParams().at(paramIndex);
                                               });
            // Add EGPs
            addChildEGPs(sg->getPSModel()->getExtraGlobalParams(), i, backend.getArrayPrefix(), "InSyn",
                         [this](size_t groupIndex, size_t childIndex)
                         {
                             return m_SortedMergedInSyns.at(groupIndex).at(childIndex).first->getPSModelTargetName();
                         });
        }
    }

    // Loop through current sources in archetypical neuron group
    for(size_t i = 0; i < getArchetype().getCurrentSources().size(); i++) {
        const auto *cs = getArchetype().getCurrentSources()[i];

        // Loop through variables
        const auto vars = cs->getCurrentSourceModel()->getVars();
        const auto &varInit = cs->getVarInitialisers();
        for(size_t v = 0; v < vars.size(); v++) {
            // Add pointers to state variable
            const auto var = vars[v];
            if(!init || !varInit[v].getSnippet()->getCode().empty()) {
                assert(!Utils::isTypePointer(var.type));
                m_Gen.addField(var.type + "*", var.name + "CS" + std::to_string(i),
                               [&backend, i, var, this](const NeuronGroupInternal &, size_t groupIndex)
                               {
                                   return backend.getArrayPrefix() + var.name + m_SortedCurrentSources.at(groupIndex).at(i)->getName();
                               });
            }

            // If we're generating an initialization structure, also add any heterogeneous parameters, derived parameters or extra global parameters required for initializers
            if(init) {
                const auto *varInitSnippet = varInit.at(v).getSnippet();
                auto getVarInitialiserFn = [this](size_t groupIndex, size_t childIndex)
                {
                    return m_SortedCurrentSources.at(groupIndex).at(childIndex)->getVarInitialisers();
                };
                addHeterogeneousChildVarInitParams(varInitSnippet->getParamNames(), i, v, var.name + "CS",
                                                   &NeuronGroupMergedBase::isCurrentSourceVarInitParamHeterogeneous, getVarInitialiserFn);
                addHeterogeneousChildVarInitDerivedParams(varInitSnippet->getDerivedParams(), i, v, var.name + "CS",
                                                          &NeuronGroupMergedBase::isCurrentSourceVarInitDerivedParamHeterogeneous, getVarInitialiserFn);
                addChildEGPs(varInitSnippet->getExtraGlobalParams(), i, backend.getArrayPrefix(), var.name + "CS",
                             [var, this](size_t groupIndex, size_t childIndex)
                             {
                                 return var.name + m_SortedCurrentSources.at(groupIndex).at(childIndex)->getName();
                             });
            }
        }

        if(!init) {
            // Add any heterogeneous current source parameters
            const auto paramNames = cs->getCurrentSourceModel()->getParamNames();
            addHeterogeneousChildParams(paramNames, i, "CS", &NeuronGroupMergedBase::isCurrentSourceParamHeterogeneous,
                                        [this](size_t groupIndex, size_t childIndex, size_t paramIndex)
                                        {
                                            return m_SortedCurrentSources.at(groupIndex).at(childIndex)->getParams().at(paramIndex);
                                        });

            // Add any heterogeneous current source derived parameters
            const auto derivedParams = cs->getCurrentSourceModel()->getDerivedParams();
            addHeterogeneousChildDerivedParams(derivedParams, i, "CS", &NeuronGroupMergedBase::isCurrentSourceDerivedParamHeterogeneous,
                                               [this](size_t groupIndex, size_t childIndex, size_t paramIndex)
                                                {
                                                    return m_SortedCurrentSources.at(groupIndex).at(childIndex)->getDerivedParams().at(paramIndex);
                                                });

            // Add EGPs
            addChildEGPs(cs->getCurrentSourceModel()->getExtraGlobalParams(), i, backend.getArrayPrefix(), "CS",
                         [this](size_t groupIndex, size_t childIndex)
                         {
                             return m_SortedCurrentSources.at(groupIndex).at(childIndex)->getName();
                         });

        }
    }

    // Loop through neuron groups
    std::vector<std::vector<SynapseGroupInternal *>> eventThresholdSGs;
    for(const auto &g : getGroups()) {
        // Reserve vector for this group's children
        eventThresholdSGs.emplace_back();

        // Add synapse groups 
        for(const auto &s : g.get().getSpikeEventCondition()) {
            if(s.egpInThresholdCode) {
                eventThresholdSGs.back().push_back(s.synapseGroup);
            }
        }
    }

    // Loop through all spike event conditions
    using FieldType = std::remove_reference<decltype(m_Gen)>::type::FieldType;
    size_t i = 0;
    for(const auto &s : getArchetype().getSpikeEventCondition()) {
        // If threshold condition references any EGPs
        if(s.egpInThresholdCode) {
            // Loop through all EGPs in synapse group and add to merged group
            // **TODO** should only be ones referenced
            const auto sgEGPs = s.synapseGroup->getWUModel()->getExtraGlobalParams();
            for(const auto &egp : sgEGPs) {
                const bool isPointer = Utils::isTypePointer(egp.type);
                const std::string prefix = isPointer ? backend.getArrayPrefix() : "";
                m_Gen.addField(egp.type, egp.name + "EventThresh" + std::to_string(i),
                               [eventThresholdSGs, prefix, egp, i](const NeuronGroupInternal &, size_t groupIndex)
                               {
                                   return prefix + egp.name + eventThresholdSGs.at(groupIndex).at(i)->getName();
                               },
                               Utils::isTypePointer(egp.type) ? FieldType::PointerEGP : FieldType::ScalarEGP);
            }
            i++;
        }
    }
}
//----------------------------------------------------------------------------
void CodeGenerator::NeuronGroupMergedBase::addMergedInSynPointerField(const std::string &type, const std::string &name, 
                                                                      size_t archetypeIndex, const std::string &prefix)
{
    assert(!Utils::isTypePointer(type));
    m_Gen.addField(type + "*", name + std::to_string(archetypeIndex),
                   [prefix, archetypeIndex, this](const NeuronGroupInternal &, size_t groupIndex)
                   {
                       return prefix + m_SortedMergedInSyns.at(groupIndex).at(archetypeIndex).first->getPSModelTargetName();
                   });
}

//----------------------------------------------------------------------------
// CodeGenerator::NeuronUpdateGroupMerged
//----------------------------------------------------------------------------
const std::string CodeGenerator::NeuronUpdateGroupMerged::name = "NeuronUpdate";
//----------------------------------------------------------------------------
CodeGenerator::NeuronUpdateGroupMerged::NeuronUpdateGroupMerged(size_t index, const std::string &precision, const std::string &timePrecision, const BackendBase &backend, 
                                                                const std::vector<std::reference_wrapper<const NeuronGroupInternal>> &groups)
:   NeuronGroupMergedBase(index, precision, timePrecision, backend, false, groups)
{
    // Build vector of vectors containing each child group's incoming synapse groups
    // with postsynaptic updates, ordered to match those of the archetype group
    orderNeuronGroupChildren(getArchetype().getInSynWithPostCode(), m_SortedInSynWithPostCode, &NeuronGroupInternal::getInSynWithPostCode,
                             [](const SynapseGroupInternal *a, const SynapseGroupInternal *b){ return a->canWUPostBeMerged(*b); });

    // Build vector of vectors containing each child group's outgoing synapse groups
    // with presynaptic synaptic updates, ordered to match those of the archetype group
    orderNeuronGroupChildren(getArchetype().getOutSynWithPreCode(), m_SortedOutSynWithPreCode, &NeuronGroupInternal::getOutSynWithPreCode,
                             [](const SynapseGroupInternal *a, const SynapseGroupInternal *b){ return a->canWUPreBeMerged(*b); });

    // Generate struct fields for incoming synapse groups with postsynaptic update code
    const auto inSynWithPostCode = getArchetype().getInSynWithPostCode();
    generateWUVar(backend, "WUPost", inSynWithPostCode, m_SortedInSynWithPostCode,
                  &WeightUpdateModels::Base::getPostVars, &NeuronUpdateGroupMerged::isInSynWUMParamHeterogeneous,
                  &NeuronUpdateGroupMerged::isInSynWUMDerivedParamHeterogeneous);

    // Generate struct fields for outgoing synapse groups with presynaptic update code
    const auto outSynWithPreCode = getArchetype().getOutSynWithPreCode();
    generateWUVar(backend, "WUPre", outSynWithPreCode, m_SortedOutSynWithPreCode,
                  &WeightUpdateModels::Base::getPreVars, &NeuronUpdateGroupMerged::isOutSynWUMParamHeterogeneous,
                  &NeuronUpdateGroupMerged::isOutSynWUMDerivedParamHeterogeneous);

}
//----------------------------------------------------------------------------
std::string CodeGenerator::NeuronUpdateGroupMerged::getCurrentQueueOffset() const
{
    assert(getArchetype().isDelayRequired());
    return "(*group->spkQuePtr * group->numNeurons)";
}
//----------------------------------------------------------------------------
std::string CodeGenerator::NeuronUpdateGroupMerged::getPrevQueueOffset() const
{
    assert(getArchetype().isDelayRequired());
    return "(((*group->spkQuePtr + " + std::to_string(getArchetype().getNumDelaySlots() - 1) + ") % " + std::to_string(getArchetype().getNumDelaySlots()) + ") * group->numNeurons)";
}
//----------------------------------------------------------------------------
bool CodeGenerator::NeuronUpdateGroupMerged::isInSynWUMParamHeterogeneous(size_t childIndex, size_t paramIndex) const
{
    // If parameter isn't referenced in code, there's no point implementing it hetereogeneously!
    const auto *wum = getArchetype().getInSynWithPostCode().at(childIndex)->getWUModel();
    const std::string paramName = wum->getParamNames().at(paramIndex);
    return isChildParamValueHeterogeneous({wum->getPostSpikeCode()}, paramName, childIndex, paramIndex, m_SortedInSynWithPostCode,
                                          [](const SynapseGroupInternal *s) { return s->getWUParams(); });
}
//----------------------------------------------------------------------------
bool CodeGenerator::NeuronUpdateGroupMerged::isInSynWUMDerivedParamHeterogeneous(size_t childIndex, size_t paramIndex) const
{
    // If derived parameter isn't referenced in code, there's no point implementing it hetereogeneously!
    const auto *wum = getArchetype().getInSynWithPostCode().at(childIndex)->getWUModel();
    const std::string derivedParamName = wum->getDerivedParams().at(paramIndex).name;
    return isChildParamValueHeterogeneous({wum->getPostSpikeCode()}, derivedParamName, childIndex, paramIndex, m_SortedInSynWithPostCode,
                                          [](const SynapseGroupInternal *s) { return s->getWUDerivedParams(); });
}
//----------------------------------------------------------------------------
bool CodeGenerator::NeuronUpdateGroupMerged::isOutSynWUMParamHeterogeneous(size_t childIndex, size_t paramIndex) const
{
    // If parameter isn't referenced in code, there's no point implementing it hetereogeneously!
    const auto *wum = getArchetype().getOutSynWithPreCode().at(childIndex)->getWUModel();
    const std::string paramName = wum->getParamNames().at(paramIndex);
    return isChildParamValueHeterogeneous({wum->getPreSpikeCode()}, paramName, childIndex, paramIndex, m_SortedOutSynWithPreCode,
                                          [](const SynapseGroupInternal *s) { return s->getWUParams(); });
}
//----------------------------------------------------------------------------
bool CodeGenerator::NeuronUpdateGroupMerged::isOutSynWUMDerivedParamHeterogeneous(size_t childIndex, size_t paramIndex) const
{
    // If derived parameter isn't referenced in code, there's no point implementing it hetereogeneously!
    const auto *wum = getArchetype().getOutSynWithPreCode().at(childIndex)->getWUModel();
    const std::string derivedParamName = wum->getDerivedParams().at(paramIndex).name;
    return isChildParamValueHeterogeneous({wum->getPreSpikeCode()}, derivedParamName, childIndex, paramIndex, m_SortedOutSynWithPreCode,
                                          [](const SynapseGroupInternal *s) { return s->getWUDerivedParams(); });
}
//----------------------------------------------------------------------------
void CodeGenerator::NeuronUpdateGroupMerged::generate(const BackendBase &backend, CodeStream &definitionsInternal,
                                                      CodeStream &definitionsInternalFunc, CodeStream &definitionsInternalVar,
                                                      CodeStream &runnerVarDecl, CodeStream &runnerMergedStructAlloc,
                                                      MergedStructData &mergedStructData, const std::string &precision,
                                                      const std::string &timePrecision) const
{
    // Generate structure definitions and instantiation
    m_Gen.generate(backend, definitionsInternal, definitionsInternalFunc, definitionsInternalVar, runnerVarDecl, runnerMergedStructAlloc,
                 mergedStructData, NeuronUpdateGroupMerged::name);
}
//----------------------------------------------------------------------------
void CodeGenerator::NeuronUpdateGroupMerged::generateWUVar(const BackendBase &backend,  const std::string &fieldPrefixStem, 
                                                           const std::vector<SynapseGroupInternal *> &archetypeSyn,
                                                           const std::vector<std::vector<SynapseGroupInternal *>> &sortedSyn,
                                                           Models::Base::VarVec (WeightUpdateModels::Base::*getVars)(void) const,
                                                           bool(NeuronUpdateGroupMerged::*isParamHeterogeneous)(size_t, size_t) const,
                                                           bool(NeuronUpdateGroupMerged::*isDerivedParamHeterogeneous)(size_t, size_t) const)
{
    // Loop through synapse groups
    for(size_t i = 0; i < archetypeSyn.size(); i++) {
        const auto *sg = archetypeSyn[i];

        // Loop through variables
        const auto vars = (sg->getWUModel()->*getVars)();
        for(size_t v = 0; v < vars.size(); v++) {
            // Add pointers to state variable
            const auto var = vars[v];
            assert(!Utils::isTypePointer(var.type));
            m_Gen.addField(var.type + "*", var.name + fieldPrefixStem + std::to_string(i),
                          [i, var, &backend, &sortedSyn](const NeuronGroupInternal &, size_t groupIndex)
                          {
                              return backend.getArrayPrefix() + var.name + sortedSyn.at(groupIndex).at(i)->getName();
                          });
        }

        // Add any heterogeneous parameters
        addHeterogeneousChildParams<NeuronUpdateGroupMerged>(sg->getWUModel()->getParamNames(), i, fieldPrefixStem, isParamHeterogeneous,
                                                             [&sortedSyn](size_t groupIndex, size_t childIndex, size_t paramIndex)
                                                             {
                                                                 return sortedSyn.at(groupIndex).at(childIndex)->getWUParams().at(paramIndex);
                                                             });

        // Add any heterogeneous derived parameters
        addHeterogeneousChildDerivedParams<NeuronUpdateGroupMerged>(sg->getWUModel()->getDerivedParams(), i, fieldPrefixStem, isDerivedParamHeterogeneous,
                                                                    [&sortedSyn](size_t groupIndex, size_t childIndex, size_t paramIndex)
                                                                    {
                                                                        return sortedSyn.at(groupIndex).at(childIndex)->getWUDerivedParams().at(paramIndex);
                                                                    });

        // Add EGPs
        addChildEGPs(sg->getWUModel()->getExtraGlobalParams(), i, backend.getArrayPrefix(), fieldPrefixStem,
                     [&sortedSyn](size_t groupIndex, size_t childIndex)
                     {
                         return sortedSyn.at(groupIndex).at(childIndex)->getName();
                     });
    }
}

//----------------------------------------------------------------------------
// CodeGenerator::NeuronInitGroupMerged
//----------------------------------------------------------------------------
const std::string CodeGenerator::NeuronInitGroupMerged::name = "NeuronInit";
//----------------------------------------------------------------------------
CodeGenerator::NeuronInitGroupMerged::NeuronInitGroupMerged(size_t index, const std::string &precision, const std::string &timePrecision, const BackendBase &backend,
                                                            const std::vector<std::reference_wrapper<const NeuronGroupInternal>> &groups)
:   NeuronGroupMergedBase(index, precision, timePrecision, backend, true, groups)
{
    // Build vector of vectors containing each child group's incoming 
    // synapse groups, ordered to match those of the archetype group
    orderNeuronGroupChildren(getArchetype().getInSynWithPostVars(), m_SortedInSynWithPostVars, &NeuronGroupInternal::getInSynWithPostVars,
                             [](const SynapseGroupInternal *a, const SynapseGroupInternal *b) { return a->canWUPostInitBeMerged(*b); });

    // Build vector of vectors containing each child group's outgoing 
    // synapse groups, ordered to match those of the archetype group
    orderNeuronGroupChildren(getArchetype().getOutSynWithPreVars(), m_SortedOutSynWithPreVars, &NeuronGroupInternal::getOutSynWithPreVars,
                             [](const SynapseGroupInternal *a, const SynapseGroupInternal *b){ return a->canWUPreInitBeMerged(*b); });

    // Generate struct fields for incoming synapse groups with postsynaptic variables
    const auto inSynWithPostVars = getArchetype().getInSynWithPostVars();
    generateWUVar(backend, "WUPost", inSynWithPostVars, m_SortedInSynWithPostVars,
                  &WeightUpdateModels::Base::getPostVars, &SynapseGroupInternal::getWUPostVarInitialisers,
                  &NeuronInitGroupMerged::isInSynWUMVarInitParamHeterogeneous,
                  &NeuronInitGroupMerged::isInSynWUMVarInitDerivedParamHeterogeneous);


    // Generate struct fields for outgoing synapse groups
    const auto outSynWithPreVars = getArchetype().getOutSynWithPreVars();
    generateWUVar(backend, "WUPre", outSynWithPreVars, m_SortedOutSynWithPreVars,
                  &WeightUpdateModels::Base::getPreVars, &SynapseGroupInternal::getWUPreVarInitialisers,
                  &NeuronInitGroupMerged::isOutSynWUMVarInitParamHeterogeneous,
                  &NeuronInitGroupMerged::isOutSynWUMVarInitDerivedParamHeterogeneous);
}
//----------------------------------------------------------------------------
bool CodeGenerator::NeuronInitGroupMerged::isInSynWUMVarInitParamHeterogeneous(size_t childIndex, size_t varIndex, size_t paramIndex) const
{
    const auto *varInitSnippet = getArchetype().getInSynWithPostVars().at(childIndex)->getWUPostVarInitialisers().at(varIndex).getSnippet();
    const std::string paramName = varInitSnippet->getParamNames().at(paramIndex);
    return isChildParamValueHeterogeneous({varInitSnippet->getCode()}, paramName, childIndex, paramIndex, m_SortedInSynWithPostVars,
                                          [varIndex](const SynapseGroupInternal *s) { return s->getWUPostVarInitialisers().at(varIndex).getParams(); });
}
//----------------------------------------------------------------------------
bool CodeGenerator::NeuronInitGroupMerged::isInSynWUMVarInitDerivedParamHeterogeneous(size_t childIndex, size_t varIndex, size_t paramIndex) const
{
    const auto *varInitSnippet = getArchetype().getInSynWithPostVars().at(childIndex)->getWUPostVarInitialisers().at(varIndex).getSnippet();
    const std::string derivedParamName = varInitSnippet->getDerivedParams().at(paramIndex).name;
    return isChildParamValueHeterogeneous({varInitSnippet->getCode()}, derivedParamName, childIndex, paramIndex, m_SortedInSynWithPostVars,
                                          [varIndex](const SynapseGroupInternal *s) { return s->getWUPostVarInitialisers().at(varIndex).getDerivedParams(); });
}
//----------------------------------------------------------------------------
bool CodeGenerator::NeuronInitGroupMerged::isOutSynWUMVarInitParamHeterogeneous(size_t childIndex, size_t varIndex, size_t paramIndex) const
{
    const auto *varInitSnippet = getArchetype().getOutSynWithPreVars().at(childIndex)->getWUPreVarInitialisers().at(varIndex).getSnippet();
    const std::string paramName = varInitSnippet->getParamNames().at(paramIndex);
    return isChildParamValueHeterogeneous({varInitSnippet->getCode()}, paramName, childIndex, paramIndex, m_SortedOutSynWithPreVars,
                                          [varIndex](const SynapseGroupInternal *s) { return s->getWUPreVarInitialisers().at(varIndex).getParams(); });
}
//----------------------------------------------------------------------------
bool CodeGenerator::NeuronInitGroupMerged::isOutSynWUMVarInitDerivedParamHeterogeneous(size_t childIndex, size_t varIndex, size_t paramIndex) const
{
    const auto *varInitSnippet = getArchetype().getOutSynWithPreVars().at(childIndex)->getWUPreVarInitialisers().at(varIndex).getSnippet();
    const std::string derivedParamName = varInitSnippet->getDerivedParams().at(paramIndex).name;
    return isChildParamValueHeterogeneous({varInitSnippet->getCode()}, derivedParamName, childIndex, paramIndex, m_SortedOutSynWithPreVars,
                                          [varIndex](const SynapseGroupInternal *s) { return s->getWUPreVarInitialisers().at(varIndex).getDerivedParams(); });
}
//----------------------------------------------------------------------------
void CodeGenerator::NeuronInitGroupMerged::generate(const BackendBase &backend, CodeStream &definitionsInternal,
                                                    CodeStream &definitionsInternalFunc, CodeStream &definitionsInternalVar,
                                                    CodeStream &runnerVarDecl, CodeStream &runnerMergedStructAlloc,
                                                    MergedStructData &mergedStructData, const std::string &precision,
                                                    const std::string &timePrecision) const
{
    

    // Generate structure definitions and instantiation
    m_Gen.generate(backend, definitionsInternal, definitionsInternalFunc, definitionsInternalVar, runnerVarDecl, runnerMergedStructAlloc,
                   mergedStructData, NeuronInitGroupMerged::name);
}
//----------------------------------------------------------------------------
void CodeGenerator::NeuronInitGroupMerged::generateWUVar(const BackendBase &backend,
                                                         const std::string &fieldPrefixStem,
                                                         const std::vector<SynapseGroupInternal *> &archetypeSyn,
                                                         const std::vector<std::vector<SynapseGroupInternal *>> &sortedSyn,
                                                         Models::Base::VarVec(WeightUpdateModels::Base::*getVars)(void) const,
                                                         const std::vector<Models::VarInit> &(SynapseGroupInternal:: *getVarInitialisers)(void) const,
                                                         bool(NeuronInitGroupMerged::*isParamHeterogeneous)(size_t, size_t, size_t) const,
                                                         bool(NeuronInitGroupMerged::*isDerivedParamHeterogeneous)(size_t, size_t, size_t) const)
{
    // Loop through synapse groups
    for(size_t i = 0; i < archetypeSyn.size(); i++) {
        const auto *sg = archetypeSyn.at(i);

        // Loop through variables
        const auto vars = (sg->getWUModel()->*getVars)();
        const auto &varInit = (sg->*getVarInitialisers)();
        for(size_t v = 0; v < vars.size(); v++) {
            // Add pointers to state variable
            const auto var = vars.at(v);
            if(!varInit.at(v).getSnippet()->getCode().empty()) {
                assert(!Utils::isTypePointer(var.type));
                m_Gen.addField(var.type + "*", var.name + fieldPrefixStem + std::to_string(i),
                               [i, var, &backend, &sortedSyn](const NeuronGroupInternal &, size_t groupIndex)
                               {
                                   return backend.getArrayPrefix() + var.name + sortedSyn.at(groupIndex).at(i)->getName();
                               });
            }

            // Also add any heterogeneous, derived or extra global parameters required for initializers
            const auto *varInitSnippet = varInit.at(v).getSnippet();
            auto getVarInitialiserFn = [&sortedSyn](size_t groupIndex, size_t childIndex)
                                       {
                                           return sortedSyn.at(groupIndex).at(childIndex)->getWUPreVarInitialisers();
                                       };
            addHeterogeneousChildVarInitParams<NeuronInitGroupMerged>(varInitSnippet->getParamNames(), i, v, var.name + fieldPrefixStem,
                                                                      isParamHeterogeneous, getVarInitialiserFn);
            addHeterogeneousChildVarInitDerivedParams<NeuronInitGroupMerged>(varInitSnippet->getDerivedParams(), i, v, var.name + fieldPrefixStem,
                                                                             isDerivedParamHeterogeneous, getVarInitialiserFn);
            addChildEGPs(varInitSnippet->getExtraGlobalParams(), i, backend.getArrayPrefix(), var.name + fieldPrefixStem,
                         [var, &sortedSyn](size_t groupIndex, size_t childIndex)
                         {
                             return var.name + sortedSyn.at(groupIndex).at(childIndex)->getName();
                         });
        }
    }
}

//----------------------------------------------------------------------------
// CodeGenerator::SynapseDendriticDelayUpdateGroupMerged
//----------------------------------------------------------------------------
const std::string CodeGenerator::SynapseDendriticDelayUpdateGroupMerged::name = "SynapseDendriticDelayUpdate";
//----------------------------------------------------------------------------
CodeGenerator::SynapseDendriticDelayUpdateGroupMerged::SynapseDendriticDelayUpdateGroupMerged(size_t index, const std::string &precision, const std::string &timePrecision, const BackendBase &backend,
                                       const std::vector<std::reference_wrapper<const SynapseGroupInternal>> &groups)
    : GroupMerged<SynapseGroupInternal, SynapseDendriticDelayUpdateGroupMerged>(index, precision, groups)
{
    m_Gen.addField("volatile unsigned int*", "denDelayPtr",
                   [&backend](const SynapseGroupInternal &sg, size_t)
                   {
                       return "getSymbolAddress(" + backend.getScalarPrefix() + "denDelayPtr" + sg.getPSModelTargetName() + ")";
                   });
}
//----------------------------------------------------------------------------
void CodeGenerator::SynapseDendriticDelayUpdateGroupMerged::generate(const BackendBase &backend, CodeStream &definitionsInternal,
                                                                     CodeStream &definitionsInternalFunc, CodeStream &definitionsInternalVar,
                                                                     CodeStream &runnerVarDecl, CodeStream &runnerMergedStructAlloc,
                                                                     MergedStructData &mergedStructData, const std::string &precision) const
{
    // Generate structure definitions and instantiation
    m_Gen.generate(backend, definitionsInternal, definitionsInternalFunc, definitionsInternalVar, runnerVarDecl, runnerMergedStructAlloc,
                 mergedStructData, SynapseDendriticDelayUpdateGroupMerged::name);
}

// ----------------------------------------------------------------------------
// CodeGenerator::SynapseConnectivityHostInitGroupMerged
//----------------------------------------------------------------------------
const std::string CodeGenerator::SynapseConnectivityHostInitGroupMerged::name = "SynapseConnectivityHostInit";
//------------------------------------------------------------------------
CodeGenerator::SynapseConnectivityHostInitGroupMerged::SynapseConnectivityHostInitGroupMerged(size_t index, const std::string &precision, const std::string &timePrecision, const BackendBase &backend,
                                                                                              const std::vector<std::reference_wrapper<const SynapseGroupInternal>> &groups)
:   GroupMerged<SynapseGroupInternal, SynapseConnectivityHostInitGroupMerged>(index, precision, groups)
{
    // **TODO** these could be generic
    m_Gen.addField("unsigned int", "numSrcNeurons",
                   [](const SynapseGroupInternal &sg, size_t) { return std::to_string(sg.getSrcNeuronGroup()->getNumNeurons()); });
    m_Gen.addField("unsigned int", "numTrgNeurons",
                   [](const SynapseGroupInternal &sg, size_t) { return std::to_string(sg.getTrgNeuronGroup()->getNumNeurons()); });
    m_Gen.addField("unsigned int", "rowStride",
                   [&backend](const SynapseGroupInternal &sg, size_t) { return std::to_string(backend.getSynapticMatrixRowStride(sg)); });

    // Add heterogeneous connectivity initialiser model parameters
    m_Gen.addHeterogeneousParams(getArchetype().getConnectivityInitialiser().getSnippet()->getParamNames(),
                                 [](const SynapseGroupInternal &sg) { return sg.getConnectivityInitialiser().getParams(); },
                                 &SynapseConnectivityHostInitGroupMerged::isConnectivityInitParamHeterogeneous);


    // Add heterogeneous connectivity initialiser derived parameters
    m_Gen.addHeterogeneousDerivedParams(getArchetype().getConnectivityInitialiser().getSnippet()->getDerivedParams(),
                                        [](const SynapseGroupInternal &sg) { return sg.getConnectivityInitialiser().getDerivedParams(); },
                                        &SynapseConnectivityHostInitGroupMerged::isConnectivityInitDerivedParamHeterogeneous);

    // Add EGP pointers to struct for both host and device EGPs
    const auto egps = getArchetype().getConnectivityInitialiser().getSnippet()->getExtraGlobalParams();
    for(const auto &e : egps) {
        m_Gen.addField(e.type + "*", e.name,
                       [e](const SynapseGroupInternal &g, size_t) { return "&" + e.name + g.getName(); });

        m_Gen.addField(e.type + "*", backend.getArrayPrefix() + e.name,
                       [e, &backend](const SynapseGroupInternal &g, size_t)
                       {
                           return "&" + backend.getArrayPrefix() + e.name + g.getName();
                       });
    }
}
//------------------------------------------------------------------------
void CodeGenerator::SynapseConnectivityHostInitGroupMerged::generate(const BackendBase &backend, CodeStream &definitionsInternal,
                                                                     CodeStream &definitionsInternalFunc, CodeStream &definitionsInternalVar,
                                                                     CodeStream &runnerVarDecl, CodeStream &runnerMergedStructAlloc,
                                                                     MergedStructData &mergedStructData, const std::string &precision) const
{
    // Generate structure definitions and instantiation
    m_Gen.generate(backend, definitionsInternal, definitionsInternalFunc, definitionsInternalVar, runnerVarDecl, runnerMergedStructAlloc,
                   mergedStructData, SynapseConnectivityHostInitGroupMerged::name, true);
}
//----------------------------------------------------------------------------
bool CodeGenerator::SynapseConnectivityHostInitGroupMerged::isConnectivityInitParamHeterogeneous(size_t paramIndex) const
{
    // If parameter isn't referenced in code, there's no point implementing it hetereogeneously!
    const auto *connectInitSnippet = getArchetype().getConnectivityInitialiser().getSnippet();
    const std::string paramName = connectInitSnippet->getParamNames().at(paramIndex);
    return isParamValueHeterogeneous({connectInitSnippet->getHostInitCode()}, paramName, paramIndex,
                                     [](const SynapseGroupInternal &sg)
                                     {
                                         return sg.getConnectivityInitialiser().getParams();
                                     });
}
//----------------------------------------------------------------------------
bool CodeGenerator::SynapseConnectivityHostInitGroupMerged::isConnectivityInitDerivedParamHeterogeneous(size_t paramIndex) const
{
    // If parameter isn't referenced in code, there's no point implementing it hetereogeneously!
    const auto *connectInitSnippet = getArchetype().getConnectivityInitialiser().getSnippet();
    const std::string paramName = connectInitSnippet->getDerivedParams().at(paramIndex).name;
    return isParamValueHeterogeneous({connectInitSnippet->getHostInitCode()}, paramName, paramIndex,
                                     [](const SynapseGroupInternal &sg)
                                     {
                                         return sg.getConnectivityInitialiser().getDerivedParams();
                                     });
}

// ----------------------------------------------------------------------------
// CodeGenerator::SynapseConnectivityInitGroupMerged
//----------------------------------------------------------------------------
const std::string CodeGenerator::SynapseConnectivityInitGroupMerged::name = "SynapseConnectivityInit";
//----------------------------------------------------------------------------
CodeGenerator::SynapseConnectivityInitGroupMerged::SynapseConnectivityInitGroupMerged(size_t index, const std::string &precision, const std::string &timePrecision, const BackendBase &backend,
                                                                                      const std::vector<std::reference_wrapper<const SynapseGroupInternal>> &groups)
:   GroupMerged<SynapseGroupInternal, SynapseConnectivityInitGroupMerged>(index, precision, groups)
{
    // **TODO** these could be generic
    m_Gen.addField("unsigned int", "numSrcNeurons",
                   [](const SynapseGroupInternal &sg, size_t) { return std::to_string(sg.getSrcNeuronGroup()->getNumNeurons()); });
    m_Gen.addField("unsigned int", "numTrgNeurons",
                   [](const SynapseGroupInternal &sg, size_t) { return std::to_string(sg.getTrgNeuronGroup()->getNumNeurons()); });
    m_Gen.addField("unsigned int", "rowStride",
                   [&backend](const SynapseGroupInternal &sg, size_t) { return std::to_string(backend.getSynapticMatrixRowStride(sg)); });

    // Add heterogeneous connectivity initialiser model parameters
    m_Gen.addHeterogeneousParams(getArchetype().getConnectivityInitialiser().getSnippet()->getParamNames(),
                                 [](const SynapseGroupInternal &sg) { return sg.getConnectivityInitialiser().getParams(); },
                                 &SynapseConnectivityInitGroupMerged::isConnectivityInitParamHeterogeneous);


    // Add heterogeneous connectivity initialiser derived parameters
    m_Gen.addHeterogeneousDerivedParams(getArchetype().getConnectivityInitialiser().getSnippet()->getDerivedParams(),
                                        [](const SynapseGroupInternal &sg) { return sg.getConnectivityInitialiser().getDerivedParams(); },
                                        &SynapseConnectivityInitGroupMerged::isConnectivityInitDerivedParamHeterogeneous);

    if(getArchetype().getMatrixType() & SynapseMatrixConnectivity::SPARSE) {
        m_Gen.addPointerField("unsigned int", "rowLength", backend.getArrayPrefix() + "rowLength");
        m_Gen.addPointerField(getArchetype().getSparseIndType(), "ind", backend.getArrayPrefix() + "ind");
    }
    else if(getArchetype().getMatrixType() & SynapseMatrixConnectivity::BITMASK) {
        m_Gen.addPointerField("uint32_t", "gp", backend.getArrayPrefix() + "gp");
    }

    // Add EGPs to struct
    m_Gen.addEGPs(getArchetype().getConnectivityInitialiser().getSnippet()->getExtraGlobalParams(),
                  backend.getArrayPrefix());

}
//----------------------------------------------------------------------------
bool CodeGenerator::SynapseConnectivityInitGroupMerged::isConnectivityInitParamHeterogeneous(size_t paramIndex) const
{
    const auto *connectivityInitSnippet = getArchetype().getConnectivityInitialiser().getSnippet();
    const std::string paramName = connectivityInitSnippet->getParamNames().at(paramIndex);
    return isParamValueHeterogeneous({connectivityInitSnippet->getRowBuildCode()}, paramName, paramIndex,
                                     [](const SynapseGroupInternal &sg) { return sg.getConnectivityInitialiser().getParams(); });
}
//----------------------------------------------------------------------------
bool CodeGenerator::SynapseConnectivityInitGroupMerged::isConnectivityInitDerivedParamHeterogeneous(size_t paramIndex) const
{
    const auto *connectivityInitSnippet = getArchetype().getConnectivityInitialiser().getSnippet();
    const std::string derivedParamName = connectivityInitSnippet->getDerivedParams().at(paramIndex).name;
    return isParamValueHeterogeneous({connectivityInitSnippet->getRowBuildCode()}, derivedParamName, paramIndex,
                                     [](const SynapseGroupInternal &sg) { return sg.getConnectivityInitialiser().getDerivedParams(); });
}
//------------------------------------------------------------------------
void CodeGenerator::SynapseConnectivityInitGroupMerged::generate(const BackendBase &backend, CodeStream &definitionsInternal,
                                                                 CodeStream &definitionsInternalFunc, CodeStream &definitionsInternalVar,
                                                                 CodeStream &runnerVarDecl, CodeStream &runnerMergedStructAlloc,
                                                                 MergedStructData &mergedStructData, const std::string &precision) const
{
    // Generate structure definitions and instantiation
    m_Gen.generate(backend, definitionsInternal, definitionsInternalFunc, definitionsInternalVar, runnerVarDecl, runnerMergedStructAlloc,
                   mergedStructData, SynapseConnectivityInitGroupMerged::name);
}

//----------------------------------------------------------------------------
// CodeGenerator::SynapseGroupMergedBase
//----------------------------------------------------------------------------
std::string CodeGenerator::SynapseGroupMergedBase::getPresynapticAxonalDelaySlot() const
{
    assert(getArchetype().getSrcNeuronGroup()->isDelayRequired());

    const unsigned int numDelaySteps = getArchetype().getDelaySteps();
    if(numDelaySteps == 0) {
        return "(*group->srcSpkQuePtr)";
    }
    else {
        const unsigned int numSrcDelaySlots = getArchetype().getSrcNeuronGroup()->getNumDelaySlots();
        return "((*group->srcSpkQuePtr + " + std::to_string(numSrcDelaySlots - numDelaySteps) + ") % " + std::to_string(numSrcDelaySlots) + ")";
    }
}
//----------------------------------------------------------------------------
std::string CodeGenerator::SynapseGroupMergedBase::getPostsynapticBackPropDelaySlot() const
{
    assert(getArchetype().getTrgNeuronGroup()->isDelayRequired());

    const unsigned int numBackPropDelaySteps = getArchetype().getBackPropDelaySteps();
    if(numBackPropDelaySteps == 0) {
        return "(*group->trgSpkQuePtr)";
    }
    else {
        const unsigned int numTrgDelaySlots = getArchetype().getTrgNeuronGroup()->getNumDelaySlots();
        return "((*group->trgSpkQuePtr + " + std::to_string(numTrgDelaySlots - numBackPropDelaySteps) + ") % " + std::to_string(numTrgDelaySlots) + ")";
    }
}
//----------------------------------------------------------------------------
std::string CodeGenerator::SynapseGroupMergedBase::getDendriticDelayOffset(const std::string &offset) const
{
    assert(getArchetype().isDendriticDelayRequired());

    if(offset.empty()) {
        return "(*group->denDelayPtr * group->numTrgNeurons) + ";
    }
    else {
        return "(((*group->denDelayPtr + " + offset + ") % " + std::to_string(getArchetype().getMaxDendriticDelayTimesteps()) + ") * group->numTrgNeurons) + ";
    }
}
//----------------------------------------------------------------------------
bool CodeGenerator::SynapseGroupMergedBase::isWUParamHeterogeneous(size_t paramIndex) const
{
    const auto *wum = getArchetype().getWUModel();
    const std::string paramName = wum->getParamNames().at(paramIndex);
    return isParamValueHeterogeneous({getArchetypeCode()}, paramName, paramIndex,
                                     [](const SynapseGroupInternal &sg) { return sg.getWUParams(); });
}
//----------------------------------------------------------------------------
bool CodeGenerator::SynapseGroupMergedBase::isWUDerivedParamHeterogeneous(size_t paramIndex) const
{
    const auto *wum = getArchetype().getWUModel();
    const std::string derivedParamName = wum->getDerivedParams().at(paramIndex).name;
    return isParamValueHeterogeneous({getArchetypeCode()}, derivedParamName, paramIndex,
                                     [](const SynapseGroupInternal &sg) { return sg.getWUDerivedParams(); });
}
//----------------------------------------------------------------------------
bool CodeGenerator::SynapseGroupMergedBase::isWUGlobalVarHeterogeneous(size_t varIndex) const
{
    // If synapse group has global WU variables
    if(getArchetype().getMatrixType() & SynapseMatrixWeight::GLOBAL) {
        const auto *wum = getArchetype().getWUModel();
        const std::string varName = wum->getVars().at(varIndex).name;
        return isParamValueHeterogeneous({getArchetypeCode()}, varName, varIndex,
                                         [](const SynapseGroupInternal &sg) { return sg.getWUConstInitVals(); });
    }
    // Otherwise, return false
    else {
        return false;
    }
}
//----------------------------------------------------------------------------
bool CodeGenerator::SynapseGroupMergedBase::isWUVarInitParamHeterogeneous(size_t varIndex, size_t paramIndex) const
{
    // If parameter isn't referenced in code, there's no point implementing it hetereogeneously!
    const auto *varInitSnippet = getArchetype().getWUVarInitialisers().at(varIndex).getSnippet();
    const std::string paramName = varInitSnippet->getParamNames().at(paramIndex);
    return isParamValueHeterogeneous({varInitSnippet->getCode()}, paramName, paramIndex,
                                     [varIndex](const SynapseGroupInternal &sg)
                                     {
                                         return sg.getWUVarInitialisers().at(varIndex).getParams();
                                     });
}
//----------------------------------------------------------------------------
bool CodeGenerator::SynapseGroupMergedBase::isWUVarInitDerivedParamHeterogeneous(size_t varIndex, size_t paramIndex) const
{
    // If derived parameter isn't referenced in code, there's no point implementing it hetereogeneously!
    const auto *varInitSnippet = getArchetype().getWUVarInitialisers().at(varIndex).getSnippet();
    const std::string derivedParamName = varInitSnippet->getDerivedParams().at(paramIndex).name;
    return isParamValueHeterogeneous({varInitSnippet->getCode()}, derivedParamName, paramIndex,
                                     [varIndex](const SynapseGroupInternal &sg)
                                     {
                                         return sg.getWUVarInitialisers().at(varIndex).getDerivedParams();
                                     });
}
//----------------------------------------------------------------------------
bool CodeGenerator::SynapseGroupMergedBase::isConnectivityInitParamHeterogeneous(size_t paramIndex) const
{
    const auto *connectivityInitSnippet = getArchetype().getConnectivityInitialiser().getSnippet();
    const std::string paramName = connectivityInitSnippet->getParamNames().at(paramIndex);
    return isParamValueHeterogeneous({connectivityInitSnippet->getRowBuildCode()}, paramName, paramIndex,
                                     [](const SynapseGroupInternal &sg) { return sg.getConnectivityInitialiser().getParams(); });
}
//----------------------------------------------------------------------------
bool CodeGenerator::SynapseGroupMergedBase::isConnectivityInitDerivedParamHeterogeneous(size_t paramIndex) const
{
    const auto *connectivityInitSnippet = getArchetype().getConnectivityInitialiser().getSnippet();
    const std::string derivedParamName = connectivityInitSnippet->getDerivedParams().at(paramIndex).name;
    return isParamValueHeterogeneous({connectivityInitSnippet->getRowBuildCode()}, derivedParamName, paramIndex,
                                     [](const SynapseGroupInternal &sg) { return sg.getConnectivityInitialiser().getDerivedParams(); });
}
//----------------------------------------------------------------------------
bool CodeGenerator::SynapseGroupMergedBase::isSrcNeuronParamHeterogeneous(size_t paramIndex) const
{
    const auto *neuronModel = getArchetype().getSrcNeuronGroup()->getNeuronModel();
    const std::string paramName = neuronModel->getParamNames().at(paramIndex);
    return isParamValueHeterogeneous({getArchetypeCode()}, paramName, paramIndex,
                                     [](const SynapseGroupInternal &sg) { return sg.getSrcNeuronGroup()->getParams(); });
}
//----------------------------------------------------------------------------
bool CodeGenerator::SynapseGroupMergedBase::isSrcNeuronDerivedParamHeterogeneous(size_t paramIndex) const
{
    const auto *neuronModel = getArchetype().getSrcNeuronGroup()->getNeuronModel();
    const std::string derivedParamName = neuronModel->getDerivedParams().at(paramIndex).name;
    return isParamValueHeterogeneous({getArchetypeCode()}, derivedParamName, paramIndex,
                                     [](const SynapseGroupInternal &sg) { return sg.getSrcNeuronGroup()->getDerivedParams(); });
}
//----------------------------------------------------------------------------
bool CodeGenerator::SynapseGroupMergedBase::isTrgNeuronParamHeterogeneous(size_t paramIndex) const
{
    const auto *neuronModel = getArchetype().getTrgNeuronGroup()->getNeuronModel();
    const std::string paramName = neuronModel->getParamNames().at(paramIndex);
    return isParamValueHeterogeneous({getArchetypeCode()}, paramName, paramIndex,
                                     [](const SynapseGroupInternal &sg) { return sg.getTrgNeuronGroup()->getParams(); });
}
//----------------------------------------------------------------------------
bool CodeGenerator::SynapseGroupMergedBase::isTrgNeuronDerivedParamHeterogeneous(size_t paramIndex) const
{
    const auto *neuronModel = getArchetype().getTrgNeuronGroup()->getNeuronModel();
    const std::string derivedParamName = neuronModel->getDerivedParams().at(paramIndex).name;
    return isParamValueHeterogeneous({getArchetypeCode()}, derivedParamName, paramIndex,
                                     [](const SynapseGroupInternal &sg) { return sg.getTrgNeuronGroup()->getDerivedParams(); });
}
//----------------------------------------------------------------------------
CodeGenerator::SynapseGroupMergedBase::SynapseGroupMergedBase(size_t index, const std::string &precision, const std::string &timePrecision, const BackendBase &backend,
                                                              Role role, const std::string &archetypeCode, const std::vector<std::reference_wrapper<const SynapseGroupInternal>> &groups)
:   GroupMerged<SynapseGroupInternal, SynapseGroupMergedBase>(index, precision, groups), m_ArchetypeCode(archetypeCode)
{
    const bool updateRole = ((role == Role::PresynapticUpdate)
                             || (role == Role::PostsynapticUpdate)
                             || (role == Role::SynapseDynamics));
    const WeightUpdateModels::Base *wum = getArchetype().getWUModel();

    m_Gen.addField("unsigned int", "rowStride",
                   [&backend](const SynapseGroupInternal &sg, size_t) { return std::to_string(backend.getSynapticMatrixRowStride(sg)); });
    if(role == Role::PostsynapticUpdate || role == Role::SparseInit) {
        m_Gen.addField("unsigned int", "colStride",
                       [](const SynapseGroupInternal &sg, size_t) { return std::to_string(sg.getMaxSourceConnections()); });
    }

    m_Gen.addField("unsigned int", "numSrcNeurons",
                   [](const SynapseGroupInternal &sg, size_t) { return std::to_string(sg.getSrcNeuronGroup()->getNumNeurons()); });
    m_Gen.addField("unsigned int", "numTrgNeurons",
                   [](const SynapseGroupInternal &sg, size_t) { return std::to_string(sg.getTrgNeuronGroup()->getNumNeurons()); });

    // If this role is one where postsynaptic input can be provided
    if(role == Role::PresynapticUpdate || role == Role::SynapseDynamics) {
        if(getArchetype().isDendriticDelayRequired()) {
            addPSPointerField(precision, "denDelay", backend.getArrayPrefix() + "denDelay");
            m_Gen.addField("volatile unsigned int*", "denDelayPtr",
                           [&backend](const SynapseGroupInternal &sg, size_t)
                           {
                               return "getSymbolAddress(" + backend.getScalarPrefix() + "denDelayPtr" + sg.getPSModelTargetName() + ")";
                           });
        }
        else {
            addPSPointerField(precision, "inSyn", backend.getArrayPrefix() + "inSyn");
        }
    }

    if(role == Role::PresynapticUpdate) {
        if(getArchetype().isTrueSpikeRequired()) {
            addSrcPointerField("unsigned int", "srcSpkCnt", backend.getArrayPrefix() + "glbSpkCnt");
            addSrcPointerField("unsigned int", "srcSpk", backend.getArrayPrefix() + "glbSpk");
        }

        if(getArchetype().isSpikeEventRequired()) {
            addSrcPointerField("unsigned int", "srcSpkCntEvnt", backend.getArrayPrefix() + "glbSpkCntEvnt");
            addSrcPointerField("unsigned int", "srcSpkEvnt", backend.getArrayPrefix() + "glbSpkEvnt");
        }
    }
    else if(role == Role::PostsynapticUpdate) {
        addTrgPointerField("unsigned int", "trgSpkCnt", backend.getArrayPrefix() + "glbSpkCnt");
        addTrgPointerField("unsigned int", "trgSpk", backend.getArrayPrefix() + "glbSpk");
    }

    // If this structure is used for updating rather than initializing
    if(updateRole) {
        // If presynaptic population has delay buffers
        if(getArchetype().getSrcNeuronGroup()->isDelayRequired()) {
            m_Gen.addField("volatile unsigned int*", "srcSpkQuePtr",
                         [&backend](const SynapseGroupInternal &sg, size_t)
                         {
                             return "getSymbolAddress(" + backend.getScalarPrefix() + "spkQuePtr" + sg.getSrcNeuronGroup()->getName() + ")";
                         });
        }

        // If postsynaptic population has delay buffers
        if(getArchetype().getTrgNeuronGroup()->isDelayRequired()) {
            m_Gen.addField("volatile unsigned int*", "trgSpkQuePtr",
                           [&backend](const SynapseGroupInternal &sg, size_t)
                           {
                               return "getSymbolAddress(" + backend.getScalarPrefix() + "spkQuePtr" + sg.getTrgNeuronGroup()->getName() + ")";
                           });
        }

        // Add heterogeneous presynaptic neuron model parameters
        m_Gen.addHeterogeneousParams(getArchetype().getSrcNeuronGroup()->getNeuronModel()->getParamNames(),
                                     [](const SynapseGroupInternal &sg) { return sg.getSrcNeuronGroup()->getParams(); },
                                     &SynapseGroupMergedBase::isSrcNeuronParamHeterogeneous);

        // Add heterogeneous presynaptic neuron model derived parameters
        m_Gen.addHeterogeneousDerivedParams(getArchetype().getSrcNeuronGroup()->getNeuronModel()->getDerivedParams(),
                                            [](const SynapseGroupInternal &sg) { return sg.getSrcNeuronGroup()->getDerivedParams(); },
                                            &SynapseGroupMergedBase::isSrcNeuronDerivedParamHeterogeneous);

        // Add heterogeneous postsynaptic neuron model parameters
        m_Gen.addHeterogeneousParams(getArchetype().getTrgNeuronGroup()->getNeuronModel()->getParamNames(),
                                     [](const SynapseGroupInternal &sg) { return sg.getTrgNeuronGroup()->getParams(); },
                                     &SynapseGroupMergedBase::isTrgNeuronParamHeterogeneous);

        // Add heterogeneous postsynaptic neuron model derived parameters
        m_Gen.addHeterogeneousDerivedParams(getArchetype().getTrgNeuronGroup()->getNeuronModel()->getDerivedParams(),
                                            [](const SynapseGroupInternal &sg) { return sg.getTrgNeuronGroup()->getDerivedParams(); },
                                            &SynapseGroupMergedBase::isTrgNeuronDerivedParamHeterogeneous);

        // Get correct code string
        const std::string code = getArchetypeCode();

        // Loop through variables in presynaptic neuron model
        const auto preVars = getArchetype().getSrcNeuronGroup()->getNeuronModel()->getVars();
        for(const auto &v : preVars) {
            // If variable is referenced in code string, add source pointer
            if(code.find("$(" + v.name + "_pre)") != std::string::npos) {
                addSrcPointerField(v.type, v.name + "Pre", backend.getArrayPrefix() + v.name);
            }
        }

        // Loop through variables in postsynaptic neuron model
        const auto postVars = getArchetype().getTrgNeuronGroup()->getNeuronModel()->getVars();
        for(const auto &v : postVars) {
            // If variable is referenced in code string, add target pointer
            if(code.find("$(" + v.name + "_post)") != std::string::npos) {
                addTrgPointerField(v.type, v.name + "Post", backend.getArrayPrefix() + v.name);
            }
        }

        // Loop through extra global parameters in presynaptic neuron model
        const auto preEGPs = getArchetype().getSrcNeuronGroup()->getNeuronModel()->getExtraGlobalParams();
        for(const auto &e : preEGPs) {
            if(code.find("$(" + e.name + "_pre)") != std::string::npos) {
                const bool isPointer = Utils::isTypePointer(e.type);
                const std::string prefix = isPointer ? backend.getArrayPrefix() : "";
                m_Gen.addField(e.type, e.name + "Pre",
                               [e, prefix](const SynapseGroupInternal &sg, size_t) { return prefix + e.name + sg.getSrcNeuronGroup()->getName(); },
                               Utils::isTypePointer(e.type) ? decltype(m_Gen)::FieldType::PointerEGP : decltype(m_Gen)::FieldType::ScalarEGP);
            }
        }

        // Loop through extra global parameters in postsynaptic neuron model
        const auto postEGPs = getArchetype().getTrgNeuronGroup()->getNeuronModel()->getExtraGlobalParams();
        for(const auto &e : postEGPs) {
            if(code.find("$(" + e.name + "_post)") != std::string::npos) {
                const bool isPointer = Utils::isTypePointer(e.type);
                const std::string prefix = isPointer ? backend.getArrayPrefix() : "";
                m_Gen.addField(e.type, e.name + "Post",
                               [e, prefix](const SynapseGroupInternal &sg, size_t) { return prefix + e.name + sg.getTrgNeuronGroup()->getName(); },
                               Utils::isTypePointer(e.type) ? decltype(m_Gen)::FieldType::PointerEGP : decltype(m_Gen)::FieldType::ScalarEGP);
            }
        }

        // Add spike times if required
        if(wum->isPreSpikeTimeRequired()) {
            addSrcPointerField(timePrecision, "sTPre", backend.getArrayPrefix() + "sT");
        }
        if(wum->isPostSpikeTimeRequired()) {
            addTrgPointerField(timePrecision, "sTPost", backend.getArrayPrefix() + "sT");
        }

        // Add heterogeneous weight update model parameters
        m_Gen.addHeterogeneousParams(wum->getParamNames(),
                                     [](const SynapseGroupInternal &sg) { return sg.getWUParams(); },
                                     &SynapseGroupMergedBase::isWUParamHeterogeneous);

        // Add heterogeneous weight update model derived parameters
        m_Gen.addHeterogeneousDerivedParams(wum->getDerivedParams(),
                                            [](const SynapseGroupInternal &sg) { return sg.getWUDerivedParams(); },
                                            &SynapseGroupMergedBase::isWUDerivedParamHeterogeneous);

        // Add pre and postsynaptic variables to struct
        m_Gen.addVars(wum->getPreVars(), backend.getArrayPrefix());
        m_Gen.addVars(wum->getPostVars(), backend.getArrayPrefix());

        // Add EGPs to struct
        m_Gen.addEGPs(wum->getExtraGlobalParams(), backend.getArrayPrefix());

        // If we're updating a group with procedural connectivity
        if(getArchetype().getMatrixType() & SynapseMatrixConnectivity::PROCEDURAL) {
            // Add heterogeneous connectivity initialiser model parameters
            m_Gen.addHeterogeneousParams(getArchetype().getConnectivityInitialiser().getSnippet()->getParamNames(),
                                         [](const SynapseGroupInternal &sg) { return sg.getConnectivityInitialiser().getParams(); },
                                         &SynapseGroupMergedBase::isConnectivityInitParamHeterogeneous);


            // Add heterogeneous connectivity initialiser derived parameters
            m_Gen.addHeterogeneousDerivedParams(getArchetype().getConnectivityInitialiser().getSnippet()->getDerivedParams(),
                                                [](const SynapseGroupInternal &sg) { return sg.getConnectivityInitialiser().getDerivedParams(); },
                                                &SynapseGroupMergedBase::isConnectivityInitDerivedParamHeterogeneous);
        }
    }

    // Add pointers to connectivity data
    if(getArchetype().getMatrixType() & SynapseMatrixConnectivity::SPARSE) {
        addWeightSharingPointerField("unsigned int", "rowLength", backend.getArrayPrefix() + "rowLength");
        addWeightSharingPointerField(getArchetype().getSparseIndType(), "ind", backend.getArrayPrefix() + "ind");

        // Add additional structure for postsynaptic access
        if(backend.isPostsynapticRemapRequired() && !wum->getLearnPostCode().empty()
           && (role == Role::PostsynapticUpdate || role == Role::SparseInit))
        {
            addWeightSharingPointerField("unsigned int", "colLength", backend.getArrayPrefix() + "colLength");
            addWeightSharingPointerField("unsigned int", "remap", backend.getArrayPrefix() + "remap");
        }

        // Add additional structure for synapse dynamics access
        if(backend.isSynRemapRequired() && !wum->getSynapseDynamicsCode().empty()
           && (role == Role::SynapseDynamics || role == Role::SparseInit))
        {
            addWeightSharingPointerField("unsigned int", "synRemap", backend.getArrayPrefix() + "synRemap");
        }
    }
    else if(getArchetype().getMatrixType() & SynapseMatrixConnectivity::BITMASK) {
        addWeightSharingPointerField("uint32_t", "gp", backend.getArrayPrefix() + "gp");
    }
    else if(getArchetype().getMatrixType() & SynapseMatrixConnectivity::PROCEDURAL) {
        m_Gen.addEGPs(getArchetype().getConnectivityInitialiser().getSnippet()->getExtraGlobalParams(),
                      backend.getArrayPrefix());
    }

    // If WU variables are procedural and this is an update or WU variables are individual
    const auto vars = wum->getVars();
    const auto &varInit = getArchetype().getWUVarInitialisers();
    const bool proceduralWeights = (getArchetype().getMatrixType() & SynapseMatrixWeight::PROCEDURAL);
    const bool individualWeights = (getArchetype().getMatrixType() & SynapseMatrixWeight::INDIVIDUAL);
    if((proceduralWeights && updateRole) || individualWeights) {
        // If we're performing a procedural update or we're initializing individual variables
        if((proceduralWeights && updateRole) || !updateRole) {
            // Add heterogeneous variable initialization parameters and derived parameters
            m_Gen.addHeterogeneousVarInitParams(wum->getVars(), &SynapseGroupInternal::getWUVarInitialisers,
                                                &SynapseGroupMergedBase::isWUVarInitParamHeterogeneous);

            m_Gen.addHeterogeneousVarInitDerivedParams(wum->getVars(), &SynapseGroupInternal::getWUVarInitialisers,
                                                       &SynapseGroupMergedBase::isWUVarInitDerivedParamHeterogeneous);
        }

        // Loop through variables
        for(size_t v = 0; v < vars.size(); v++) {
            // If we're updating or if there is initialization code for this variable 
            // (otherwise, it's not needed during initialization)
            const auto var = vars[v];
            if(individualWeights && (updateRole || !varInit.at(v).getSnippet()->getCode().empty())) {
                addWeightSharingPointerField(var.type, var.name, backend.getArrayPrefix() + var.name);
            }

            // If we're performing a procedural update or we're initializing, add any var init EGPs to structure
            if((proceduralWeights && updateRole) || !updateRole) {
                const auto egps = varInit.at(v).getSnippet()->getExtraGlobalParams();
                for(const auto &e : egps) {
                    const bool isPointer = Utils::isTypePointer(e.type);
                    const std::string prefix = isPointer ? backend.getArrayPrefix() : "";
                    m_Gen.addField(e.type, e.name + var.name,
                                   [e, prefix, var](const SynapseGroupInternal &sg, size_t)
                                   {
                                       if(sg.isWeightSharingSlave()) {
                                           return prefix + e.name + var.name + sg.getWeightSharingMaster()->getName();
                                       }
                                       else {
                                           return prefix + e.name + var.name + sg.getName();
                                       }

                                   },
                                   isPointer ? decltype(m_Gen)::FieldType::PointerEGP : decltype(m_Gen)::FieldType::ScalarEGP);
                }
            }
        }
    }
    // Otherwise, if WU variables are global and this is an update kernel
    // **NOTE** global variable values aren't useful during initialization
    else if(getArchetype().getMatrixType() & SynapseMatrixWeight::GLOBAL && updateRole) {
        for(size_t v = 0; v < vars.size(); v++) {
            // If variable should be implemented heterogeneously, add scalar field
            if(isWUGlobalVarHeterogeneous(v)) {
                m_Gen.addScalarField(vars[v].name,
                                     [v](const SynapseGroupInternal &sg, size_t)
                                     {
                                         return Utils::writePreciseString(sg.getWUConstInitVals().at(v));
                                     });
            }
        }
    }
}
//----------------------------------------------------------------------------
void CodeGenerator::SynapseGroupMergedBase::generate(const BackendBase &backend, CodeStream &definitionsInternal,
                                                     CodeStream &definitionsInternalFunc, CodeStream &definitionsInternalVar,
                                                     CodeStream &runnerVarDecl, CodeStream &runnerMergedStructAlloc,
                                                     MergedStructData &mergedStructData, const std::string &precision, 
                                                     const std::string &timePrecision, const std::string &name, Role role) const
{
   

    // Generate structure definitions and instantiation
    m_Gen.generate(backend, definitionsInternal, definitionsInternalFunc, definitionsInternalVar, runnerVarDecl, runnerMergedStructAlloc,
                   mergedStructData, name);
}
//----------------------------------------------------------------------------
void CodeGenerator::SynapseGroupMergedBase::addPSPointerField(const std::string &type, const std::string &name, const std::string &prefix)
{
    assert(!Utils::isTypePointer(type));
    m_Gen.addField(type + "*", name, [prefix](const SynapseGroupInternal &sg, size_t) { return prefix + sg.getPSModelTargetName(); });
}
//----------------------------------------------------------------------------
void CodeGenerator::SynapseGroupMergedBase::addSrcPointerField(const std::string &type, const std::string &name, const std::string &prefix)
{
    assert(!Utils::isTypePointer(type));
    m_Gen.addField(type + "*", name, [prefix](const SynapseGroupInternal &sg, size_t) { return prefix + sg.getSrcNeuronGroup()->getName(); });
}
//----------------------------------------------------------------------------
void CodeGenerator::SynapseGroupMergedBase::addTrgPointerField(const std::string &type, const std::string &name, const std::string &prefix)
{
    assert(!Utils::isTypePointer(type));
    m_Gen.addField(type + "*", name, [prefix](const SynapseGroupInternal &sg, size_t) { return prefix + sg.getTrgNeuronGroup()->getName(); });
}
//----------------------------------------------------------------------------
void CodeGenerator::SynapseGroupMergedBase::addWeightSharingPointerField(const std::string &type, const std::string &name, const std::string &prefix)
{
    assert(!Utils::isTypePointer(type));
    m_Gen.addField(type + "*", name, 
                   [prefix](const SynapseGroupInternal &sg, size_t)
                   { 
                       if(sg.isWeightSharingSlave()) {
                           return prefix + sg.getWeightSharingMaster()->getName();
                       }
                       else {
                           return prefix + sg.getName();
                       }
                   });
}

//----------------------------------------------------------------------------
// CodeGenerator::PresynapticUpdateGroupMerged
//----------------------------------------------------------------------------
const std::string CodeGenerator::PresynapticUpdateGroupMerged::name = "PresynapticUpdate";

//----------------------------------------------------------------------------
// CodeGenerator::PostsynapticUpdateGroupMerged
//----------------------------------------------------------------------------
const std::string CodeGenerator::PostsynapticUpdateGroupMerged::name = "PostsynapticUpdate";

//----------------------------------------------------------------------------
// CodeGenerator::SynapseDynamicsGroupMerged
//----------------------------------------------------------------------------
const std::string CodeGenerator::SynapseDynamicsGroupMerged::name = "SynapseDynamics";

//----------------------------------------------------------------------------
// CodeGenerator::SynapseDenseInitGroupMerged
//----------------------------------------------------------------------------
const std::string CodeGenerator::SynapseDenseInitGroupMerged::name = "SynapseDenseInit";

//----------------------------------------------------------------------------
// CodeGenerator::SynapseSparseInitGroupMerged
//----------------------------------------------------------------------------
const std::string CodeGenerator::SynapseSparseInitGroupMerged::name = "SynapseSparseInit";