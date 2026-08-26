## Analysis
We have OBVIOUSLY a kernel challenge this time different because we have two vm

visibly the first vm we ssh into then can connect to the second vm so the goal is to exploit the second vm and get a root shell there

There is a module named netsec.ko that we have the code for

## Code analysis
```C
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
```This is netsec.h 

basically define our cache size to be 0X20 and key size
define a 8 bit hash for addr/port combinaison and IPADDR allows to get the four bytes of the addr

define a sec_conn struct simple with in_key, out_key, buf and buf_len addresses

## Netsec.c
It starts by giving us a 0X100 hash_table for the sec_conn objects
also stores a bitearray for the inkey 

the init start by doing crypto_kdf before registering both in and out

crypto_kdf just generates a keystream

inkey et outkey sont calculés par rapport au port de connection

for in

if syn it calls create_sec_conn
if FIN it calls destroy_sec_conn

for in and out if data_len>0
sconn = get_sec_conn
then calls get_sec_conn_buf
then
skb_copy_bits
crypto_xor
skb_store_bits

create_sec_conn
allocates a sconn struct in the cache
in_key = in_key
out_key is pointer to allocation
buf = null
and buf_len = 0

then does crypto_kdf on the out_key
stores the sconn in the hashtable

get_sec_conn returns the entry in the hastable based on ip/port hash

get_sec_conn_buf does kmem_cache_alloc if len buf <= 0x20
otherwise it does a kmalloc

tyhen basically what it does is xor the buf


destroy_sec_conn

it kmem_cache_free the out_key and sconn
but kfree the buf

UAF cause it doesn't null the hash table which still has our entry
hash collision aussi vu que peut y avoir deux memes hash

sans kaslr

addr module = 0xffffffffc0000000

hash_table = 0xffffffffc02015c0

in_key = 0xffffffffc0201dc0

note that leaking 
in_key addr -> leak .text ? defeat kaslr
out_key heap leak
buf heap

when u free empty buf sconn the buf then points directly to the out_key cos of the linked list

if u reclaim c with 1 0 it will take the freed sconn buffer so buf -> sconn freed

1. spray and use rst and not fin we want our next chunk next to each other
2. claim C
3. abuse the hash collision A = B
4. free A -> B closes
5. reclaim with C now buffer C point to freed B
6. send 33 null bytes on B it will thus send us back the data left on the chunk = heap leak
7. use C to gain arbitrary read by changing first qword then sending 8 null bytes on B (note that C data need be xored with both keys)
8. leak in_key_global (we will need it for arb write and give module .data leak)
9. leak kernel .data it's in module .data
10. use it to leak core_pattern and init_task and init_cred
11. walk the list of tasks to find cat task
12. we need overwrite its creds and real creds to init_cred
13. since we caén't locally trigger crash is why we do that and abuse signals and flagqs
14. put signal to sigsev flags to TIF_SIGPENDING
15. overwrite core_pattern with reverse shell

