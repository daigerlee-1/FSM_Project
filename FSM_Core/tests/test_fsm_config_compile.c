/* Compile-only fixture: declared arrays need no storage or target callbacks. */
#include "fsm.h"

#ifndef TEST_OPERATION_COUNT
#define TEST_OPERATION_COUNT 2U
#endif

#ifndef TEST_STATE_COUNT
#define TEST_STATE_COUNT 2U
#endif

#ifndef TEST_TRANSITION_COUNT
#define TEST_TRANSITION_COUNT 2U
#endif

#ifndef TEST_EVENT_COUNT
#define TEST_EVENT_COUNT 3U
#endif

extern const Fsm_StateOperationType Compile_Operations[TEST_OPERATION_COUNT];
extern const Fsm_TransitionType Compile_Transitions[TEST_TRANSITION_COUNT];

FSM_DEFINE_CONFIG(Compile_Config, 0U, TEST_STATE_COUNT, TEST_EVENT_COUNT,
                  Compile_Operations, Compile_Transitions);

int main(void)
{
    /* Keep the generated configuration referenced with strict warnings. */
    return ((Compile_Config.stateCount == TEST_STATE_COUNT) &&
            (Compile_Config.transitionCount == TEST_TRANSITION_COUNT) &&
            (Compile_Config.eventCount == TEST_EVENT_COUNT)) ? 0 : 1;
}
