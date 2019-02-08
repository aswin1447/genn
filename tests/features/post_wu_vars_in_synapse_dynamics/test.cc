//--------------------------------------------------------------------------
/*! \file post_wu_vars_in_synapse_dynamics/test.cc

\brief Main test code that is part of the feature testing
suite of minimal models with known analytic outcomes that are used for continuous integration testing.
*/
//--------------------------------------------------------------------------


// Google test includes
#include "gtest/gtest.h"

// Autogenerated simulation code includess
#include DEFINITIONS_HEADER

// This test does't support building for GPU and testing on CPU
#define CPU_GPU_NOT_SUPPORTED

// **NOTE** base-class for simulation tests must be
// included after auto-generated globals are includes
#include "../../utils/simulation_test.h"

// Combine neuron and synapse policies together to build variable-testing fixture
class SimTest : public SimulationTestModern
{
public:
    void Simulate()
    {
        while(t < 200.0f) {
            StepGeNN();

            // Ignore first timestep as no presynaptic events will be processed so wsyn is in it's initial state
            if(t > DT) {
                // Loop through neurons
                for(unsigned int i = 0; i < 10; i++) {
                    // Calculate time of spikes we SHOULD be reading
                    // **NOTE** we delay by 22 timesteps because:
                    // 1) delay = 20
                    // 2) spike times are read in presynaptic kernel one timestep AFTER being emitted
                    // 3) t is incremented one timestep at te end of StepGeNN
                    const float delayedTime = (scalar)i + (10.0f * std::floor((t - 22.0f - (scalar)i) / 10.0f));

                    // If, theoretically, spike would have arrived before delay it's impossible so time should be a very large negative number
                    if(delayedTime < 0.0f) {
                        ASSERT_LT(wsyn[i], -1.0E6);
                    }
                    else {
                        ASSERT_FLOAT_EQ(wsyn[i], delayedTime);
                    }
                }
            }
        }
    }
};

TEST_F(SimTest, AcceptableError)
{
    Simulate();
}
