#ifndef NETSEC_H_
#define NETSEC_H_

#define CACHE_SIZE 0x20
#define KEY_SIZE CACHE_SIZE
#define HASH(addr, port) ((addr >> 24) ^ (addr >> 16) ^ (addr >> 8) ^ (addr) ^ (port >> 8)  ^ (port)) & 0xff
#define IPADDR(addr) ((uint8_t*)&addr)[3], ((uint8_t*)&addr)[2], ((uint8_t*)&addr)[1], ((uint8_t*)&addr)[0]

struct sec_conn {
    uint8_t* in_key;
    uint8_t* out_key;
    uint8_t* buf;
    uint buf_len;
};

void crypto_xor(uint8_t* buf, size_t buf_len, uint8_t* key, size_t key_len);
void crypto_kdf(uint8_t* key, size_t key_len, uint32_t seed);

void create_sec_conn(uint32_t ip_src, uint16_t port_src);
void destroy_sec_conn(uint32_t ip_src, uint16_t port_src);
struct sec_conn* get_sec_conn(uint32_t ip_src, uint16_t port_src);
uint8_t* get_sec_conn_buf(struct sec_conn* sconn, size_t len);

unsigned int hook_in(void *priv, struct sk_buff *skb, const struct nf_hook_state *state);
unsigned int hook_out(void *priv, struct sk_buff *skb, const struct nf_hook_state *state);

#endif