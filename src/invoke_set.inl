/*
** invoke_set.inl — Set method dispatch for OP_INVOKE.
** Included inside CASE(OP_INVOKE) when receiver is OBJ_SET.
**
** Available variables: base, arg_count, receiver, mname, mlen, args, R, K
*/

ObjSet *set = as_set(receiver);

if (mlen == 3 && memcmp(mname, "add", 3) == 0)
{
    /* set.add(val) → returns true if newly added */
    if (arg_count != 1) { runtime_error("add() expects 1 argument"); return; }
    R[base] = val_bool(set_add(&gc_, set, args[0]));
}
else if (mlen == 3 && memcmp(mname, "has", 3) == 0)
{
    /* set.has(val) → bool */
    if (arg_count != 1) { runtime_error("has() expects 1 argument"); return; }
    R[base] = val_bool(set_contains(set, args[0]));
}
else if (mlen == 6 && memcmp(mname, "delete", 6) == 0)
{
    /* set.delete(val) → returns true if was present */
    if (arg_count != 1) { runtime_error("delete() expects 1 argument"); return; }
    R[base] = val_bool(set_remove(set, args[0]));
}
else if (mlen == 4 && memcmp(mname, "size", 4) == 0)
{
    /* set.size() → number of elements */
    R[base] = val_int(set->count);
}
else if (mlen == 5 && memcmp(mname, "clear", 5) == 0)
{
    /* set.clear() → remove all elements */
    set_clear(&gc_, set);
    R[base] = val_nil();
}
else if (mlen == 6 && memcmp(mname, "values", 6) == 0)
{
    /* set.values() → array of all values */
    ObjArray *result = new_array(&gc_);
    for (int32_t si = 0; si < set->capacity; si++) {
        if (!is_nil(set->nodes[si].key)) {
            array_push(&gc_, result, set->nodes[si].key);
        }
    }
    R[base] = val_obj((Obj *)result);
}
else
{
    runtime_error("set has no method '%s'", mname);
    return;
}
