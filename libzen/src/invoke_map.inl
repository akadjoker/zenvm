/*
** invoke_map.inl — Map method dispatch for OP_INVOKE.
** Included inside CASE(OP_INVOKE) when receiver is OBJ_MAP.
**
** Available variables: base, arg_count, receiver, mname, mlen, args, R, K
*/

ObjMap *map = as_map(receiver);

if (mlen == 3 && memcmp(mname, "set", 3) == 0)
{
    /* map.set(key, val) → sets key, returns val */
    if (arg_count != 2)
    {
        runtime_error("set() expects (key, value)");
        return;
    }
    map_set(&gc_, map, args[0], args[1]);
    R[base] = args[1];
}
else if (mlen == 3 && memcmp(mname, "get", 3) == 0)
{
    /* map.get(key) or map.get(key, default) → value or nil/default */
    if (arg_count < 1)
    {
        runtime_error("get() expects a key");
        return;
    }
    bool found;
    Value result = map_get(map, args[0], &found);
    if (found)
        R[base] = result;
    else
        R[base] = (arg_count >= 2) ? args[1] : val_nil();
}
else if (mlen == 3 && memcmp(mname, "has", 3) == 0)
{
    /* map.has(key) → bool */
    if (arg_count != 1)
    {
        runtime_error("has() expects a key");
        return;
    }
    R[base] = val_bool(map_contains(map, args[0]));
}
else if (mlen == 6 && memcmp(mname, "delete", 6) == 0)
{
    /* map.delete(key) → removes key, returns true if existed */
    if (arg_count != 1)
    {
        runtime_error("delete() expects a key");
        return;
    }
    R[base] = val_bool(map_delete(map, args[0]));
}
else if (mlen == 4 && memcmp(mname, "keys", 4) == 0)
{
    /* map.keys() → array of keys */
    ObjArray *result = new_array(&gc_);
    map_keys(&gc_, map, result);
    R[base] = val_obj((Obj *)result);
}
else if (mlen == 6 && memcmp(mname, "values", 6) == 0)
{
    /* map.values() → array of values */
    ObjArray *result = new_array(&gc_);
    map_values(&gc_, map, result);
    R[base] = val_obj((Obj *)result);
}
else if (mlen == 4 && memcmp(mname, "size", 4) == 0)
{
    /* map.size() → number of entries */
    R[base] = val_int(map->count);
}
else if (mlen == 5 && memcmp(mname, "clear", 5) == 0)
{
    /* map.clear() → remove all entries */
    map_clear(&gc_, map);
    R[base] = val_nil();
}
else
{
    runtime_error("map has no method '%s'", mname);
    return;
}
