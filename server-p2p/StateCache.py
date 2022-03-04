from dict_helpers import *

"""
Fast state cache implementation.

Contains all runners and which states they have prefetched

Format (by state):
    {state_id: list(runner_id,...)}
"""
class StateCache:
    c = {} # FIXME: might be faster with [] as inner container!
    cr = {}
    splits = {}

    @staticmethod
    def add(state_id, runner_id):
        dict_append(StateCache.c, state_id, runner_id)
        dict_append(StateCache.cr, runner_id, state_id)
        StateCache.splits[state_id] = len(StateCache.c[state_id])


    @staticmethod
    def remove(state_id, runner_id):
        dict_remove(StateCache.c, state_id, runner_id)
        dict_remove(StateCache.cr, runner_id, state_id)
        if state_id in StateCache.c:
            StateCache.splits[state_id] = len(StateCache.c[state_id])
        else:
            if state_id in StateCache.splits:
                del StateCache.splits[state_id]


    @staticmethod
    def get_runners(state_id):
        if state_id in StateCache.c:
            return StateCache.c[state_id]
        else:
            return []

    @staticmethod
    def get_by_runner(runner_id):
        if runner_id in StateCache.cr:
            return StateCache.cr[runner_id]
        else:
            return []

    @staticmethod
    def get_splits():
        # should return the same as {idx: len(runners) for idx, runners in StateCache.c.items()}
        return StateCache.splits



    @staticmethod
    def update(msg, runner_id):
        # Update knowledge about state caches of runners:
        # This is the operation probably quite often and it is freaking slow!
        for state_id in StateCache.cr[runner_id]:
            dict_remove(StateCache.c, state_id, runner_id)
            if state_id in StateCache.c:
                StateCache.splits[state_id] = len(StateCache.c[state_id])
            else:
                if state_id in StateCache.splits:
                    del StateCache.splits[state_id]
        StateCache.cr[runner_id] = msg.cached_states
        for state_id in msg.cached_states:
            dict_append(StateCache.c, state_id, runner_id)
            StateCache.splits[state_id] = len(StateCache.c[state_id])
