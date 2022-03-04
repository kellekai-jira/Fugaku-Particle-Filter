from StateCache import StateCache

"""
    select a new good parent particle to propagate on a runner with id runner_id

    This function is written in a way compatible with the server.py implementation!

    @param runner_id select parent for this runner
    @param alpha dict representing which parent still needs to be propagated how many times
    @param len_alpha variable containing the length of alpha

    @returns p, cachehit p the parent that was selected and if it was a cachehit or not
"""
def select_parent(runner_id, alpha, len_alpha, state_cache, R):
    for p in state_cache.get_by_runner(runner_id):
        if p in alpha:
            # TODO: maybe one wants to see if runners_last works fine.... so cache gets destroyed less?
            found_to_schedule = True
            return p, True


    # now do the split factor thing:
    Q = len_alpha / R

    split_factors_bad = {}  # try to schedule from this with lowest prio, stuff you may not split but we split it anyway before doing nothing
    split_factors = {}      # next highest prio, stuff that may be used regarding the split factor
    split_factors_new = {}  # highest prio, stuff that is not cached on any runner so far to possibly worksteal less...
    # very highest prio is stuff that is already in the cache. this is managed up there.

    # REM: within all those groups we try to select the one with the highest split factor (except for the very highes prio group.)

    splits = state_cache.get_splits()

    for p in alpha:
        split_factor = alpha[p] / Q

        if p in splits:
            if split_factor > splits[p]:
                split_factors[p] = split_factor
            else:
                split_factors_bad[p] = split_factor
        else:
            split_factors_new[p] = split_factor


    if len(split_factors_new) > 0:
        p = max(split_factors_new, key=split_factors_new.get)
    elif len(split_factors) > 0:
        p = max(split_factors, key=split_factors.get)
    elif len(split_factors_bad) > 0:
        p = max(split_factors_bad, key=split_factors_bad.get)
    else:
        return None, None

    return p, False


"""

"""
def select_evict(delete_from, alpha, state_weights, assimilation_cycle):
    too_old = []    # delete state that is too old --> O(#state cache)
    not_important = []  # delete state that is not important for any job this iteration anymore --> O(#state cache * #unscheduled jobs) , hashmaps might help?
    too_new = []  # delete state that is too new --> O(#state cache)
    even_newer = [] # stuff that was propagated speculatively - delete this first before too new

    for s in delete_from:
        if s.t < assimilation_cycle - 1:
            too_old.append(s)
            break

        elif s.t == assimilation_cycle - 1 and s not in alpha:
            not_important.append(s)

        elif s.t == assimilation_cycle and s in state_weights:
            too_new.append(s)

        elif s.t > assimilation_cycle and s in state_weights:
            even_newer.append(s)


    all_lists = too_old + not_important


    if len(all_lists) == 0:
        if len(even_newer) != 0:
            to_delete = even_newer[0]
            lowest_weight = state_weights[to_delete]
            for en in even_newer[1:]:
                w = state_weights[en]
                if w < lowest_weight:
                    to_delete = en
                    lowest_weight = w

        elif len(too_new) != 0:
            # delete new state with lowest weight
            to_delete = too_new[0]
            lowest_weight = state_weights[to_delete]
            for tn in too_new[1:]:
                w = state_weights[tn]
                if w < lowest_weight:
                    to_delete = tn
                    lowest_weight = w

        else:
            # delete something randomly
            # to_delete = np.random.choice(delete_from)
            if len(delete_from) == 0:
                print('Shall delete but cannot find something randomly to delete', msg)
                print('States on this runner (to delete from):', msg.delete_request.cached_states)
                print('blacklist:', blacklist)
                assert False
            to_delete = delete_from[0]
    else:
        to_delete = all_lists[0]  # take first possible...

    return to_delete
