def dict_append(d, where, what):
    if where not in d:
        d[where] = []
    d[where].append(what)

def dict_remove(d, where, what):
    d[where].remove(what)
    if len(d[where]) == 0:
        del d[where]

