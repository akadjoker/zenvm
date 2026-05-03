/*
** invoke_map.inl — Map method dispatch for OP_INVOKE.
** Included inside CASE(OP_INVOKE) when receiver is OBJ_MAP.
**
** Available variables: base, arg_count, receiver, mname, mlen, args, R, K
*/

ObjMap *map = as_map(receiver);

switch (method->map_method_id)
{
case MAP_SET:
{
    /* map.set(key, val) → sets key, returns val */
    if (arg_count != 2)
    {
        runtime_error("set() expects (key, value)");
        return;
    }
    map_set(&gc_, map, args[0], args[1]);
    R[base] = args[1];
    break;
}
case MAP_GET:
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
    break;
}
case MAP_HAS:
{
    /* map.has(key) → bool */
    if (arg_count != 1)
    {
        runtime_error("has() expects a key");
        return;
    }
    R[base] = val_bool(map_contains(map, args[0]));
    break;
}
case MAP_DELETE:
{
    /* map.delete(key) → removes key, returns true if existed */
    if (arg_count != 1)
    {
        runtime_error("delete() expects a key");
        return;
    }
    R[base] = val_bool(map_delete(map, args[0]));
    break;
}
case MAP_KEYS:
{
    /* map.keys() → array of keys */
    ObjArray *result = new_array(&gc_);
    map_keys(&gc_, map, result);
    R[base] = val_obj((Obj *)result);
    break;
}
case MAP_VALUES:
{
    /* map.values() → array of values */
    ObjArray *result = new_array(&gc_);
    map_values(&gc_, map, result);
    R[base] = val_obj((Obj *)result);
    break;
}
case MAP_SIZE:
{
    /* map.size() → number of entries */
    R[base] = val_int(map->count);
    break;
}
case MAP_CLEAR:
{
    /* map.clear() → remove all entries */
    map_clear(&gc_, map);
    R[base] = val_nil();
    break;
}
default:
{
    runtime_error("map has no method '%s'", mname);
    return;
}
}
