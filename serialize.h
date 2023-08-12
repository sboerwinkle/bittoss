extern void write32(list<char> *data, int32_t v);
extern void serialize(gamestate *gs, list<char> *data);
extern void serializeSelected(gamestate *gs, list<char> *data, const int32_t *c_offset, const int32_t *v_offset);
extern void deserialize(gamestate *gs, const list<char> *data);
extern void deserializeSelected(gamestate *gs, const list<char> *data, const int32_t *c_offset, const int32_t *v_offset);
