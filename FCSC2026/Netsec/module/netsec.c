#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/netfilter.h>
#include <linux/netfilter_ipv4.h>
#include <linux/ip.h>
#include <linux/tcp.h>

#include "netsec.h"



static int hook_port = 0;
static struct kmem_cache *cache = NULL;
static uint8_t in_key[KEY_SIZE] = { 0 };
static struct sec_conn* hash_table[0x100] = { NULL };

static struct nf_hook_ops in_ops = {
    .hook = hook_in,
    .pf = PF_INET,
    .hooknum = NF_INET_LOCAL_IN,
    .priority = NF_IP_PRI_LAST,
};

static struct nf_hook_ops out_ops = {
    .hook = hook_out,
    .pf = PF_INET,
    .hooknum = NF_INET_LOCAL_OUT,
    .priority = NF_IP_PRI_LAST,
};



void crypto_kdf(uint8_t* key, size_t key_len, uint32_t seed) {
    if (key) {
        uint32_t state = seed * 0xdeadbeef + 0x12345;
        for (int i = 0; i < (key_len / 4); i++) {
            state ^= state << 13;
            state ^= state >> 17;
            state ^= state << 5;
            ((uint32_t*)key)[i] = state;
        }
    }
}

void crypto_xor(uint8_t* buf, size_t buf_len, uint8_t* key, size_t key_len) {
    if (buf && key) {
        for (int i = 0; i < buf_len; i++) {
            buf[i] ^= key[i % key_len];
        }
    }
}



void create_sec_conn(uint32_t ip, uint16_t port) {
    uint8_t h = HASH(ip, port);
    struct sec_conn* sconn = kmem_cache_alloc(cache, GFP_KERNEL);
    sconn->in_key = in_key;
    sconn->out_key = kmem_cache_alloc(cache, GFP_KERNEL);
    sconn->buf = NULL;
    sconn->buf_len = 0;
    crypto_kdf(sconn->out_key, KEY_SIZE, port);
    hash_table[h] = sconn;
}

void destroy_sec_conn(uint32_t ip, uint16_t port) {
    uint8_t h = HASH(ip, port);
    struct sec_conn* sconn = hash_table[h];
    if (sconn) {
        if (sconn->out_key) {
            kmem_cache_free(cache, sconn->out_key);
        }
        if (sconn->buf) {
            kfree(sconn->buf);
        }
        kmem_cache_free(cache, sconn);
    }
}

struct sec_conn* get_sec_conn(uint32_t ip, uint16_t port) {
    uint8_t h = HASH(ip, port);
    return hash_table[h];
}

uint8_t* get_sec_conn_buf(struct sec_conn* sconn, size_t len) {
    if (sconn->buf_len < len) {
        kfree(sconn->buf);
        if (len <= CACHE_SIZE) {
            len = CACHE_SIZE;
            sconn->buf = kmem_cache_alloc(cache, GFP_KERNEL);
        } else {
            sconn->buf = kmalloc(len, GFP_KERNEL);
        }
        sconn->buf_len = len;
    }
    return sconn->buf;
}



unsigned int hook_in(void *priv, struct sk_buff *skb, const struct nf_hook_state *state) {
    
    uint32_t ip_src;
    uint32_t ip_dst;
    uint16_t port_src;
    uint16_t port_dst;
    struct iphdr* iph;
    struct tcphdr* tcph;
    uint header_len;
    uint data_len;
    struct sec_conn* sconn;
    uint8_t* buf;

    if (!skb) {
        return NF_ACCEPT;
    }
    
    iph = ip_hdr(skb);
    ip_src = ntohl(iph->saddr);
    ip_dst = ntohl(iph->daddr);
    if (iph->protocol != IPPROTO_TCP) {
        return NF_ACCEPT;
    }

    tcph = tcp_hdr(skb);
    port_src = ntohs(tcph->source);
    port_dst = ntohs(tcph->dest);
    if (port_dst != hook_port) {
        return NF_ACCEPT;
    }

    header_len = (iph->ihl + tcph->doff) * 4;
    data_len = skb->len - header_len;

    printk(KERN_INFO "IN: %u.%u.%u.%u:%u -> %u.%u.%u.%u:%u S:%u A:%u F:%u R:%u [0x%x|0x%x]\n", IPADDR(ip_src), port_src, IPADDR(ip_dst), port_dst, tcph->syn, tcph->ack, tcph->fin, tcph->rst, header_len, data_len);

    if (tcph->syn) {
        // new TCP connection
        create_sec_conn(ip_src, port_src);
    } else if (tcph->fin) {
        // end of TCP connection
        destroy_sec_conn(ip_src, port_src);
    } else {
        if (data_len > 0) {
        // data in TCP payload
            sconn = get_sec_conn(ip_src, port_src);
            if (sconn) {
                buf = get_sec_conn_buf(sconn, data_len);
                if (buf) {
                    skb_copy_bits(skb, header_len, buf, data_len);
                    crypto_xor(buf, data_len, sconn->in_key, KEY_SIZE);
                    skb_store_bits(skb, header_len, buf, data_len);
                }
            }
        }
    }

    return NF_ACCEPT;
}



unsigned int hook_out(void *priv, struct sk_buff *skb, const struct nf_hook_state *state) {
    
    uint32_t ip_src;
    uint32_t ip_dst;
    uint16_t port_src;
    uint16_t port_dst;
    struct iphdr* iph;
    struct tcphdr* tcph;
    uint header_len;
    uint data_len;
    struct sec_conn* sconn;
    uint8_t* buf;

    if (!skb) {
        return NF_ACCEPT;
    }
    
    iph = ip_hdr(skb);
    ip_src = ntohl(iph->saddr);
    ip_dst = ntohl(iph->daddr);
    if (iph->protocol != IPPROTO_TCP) {
        return NF_ACCEPT;
    }

    tcph = tcp_hdr(skb);
    port_src = ntohs(tcph->source);
    port_dst = ntohs(tcph->dest);
    if (port_src != hook_port) {
        return NF_ACCEPT;
    }

    header_len = (iph->ihl + tcph->doff) * 4;
    data_len = skb->len - header_len;
    
    printk(KERN_INFO "OUT: %u.%u.%u.%u:%u -> %u.%u.%u.%u:%u S:%u A:%u F:%u R:%u [0x%x|0x%x]\n", IPADDR(ip_src), port_src, IPADDR(ip_dst), port_dst, tcph->syn, tcph->ack, tcph->fin, tcph->rst, header_len, data_len);

    if (tcph->syn || tcph->fin) {
        return NF_ACCEPT;
    } else {
        if (data_len > 0) {
            // data in TCP payload
            sconn = get_sec_conn(ip_dst, port_dst);
            if (sconn) {
                buf = get_sec_conn_buf(sconn, data_len);
                if (buf) {
                    skb_copy_bits(skb, header_len, buf, data_len);
                    crypto_xor(buf, data_len, sconn->out_key, KEY_SIZE);
                    skb_store_bits(skb, header_len, buf, data_len);
                }
            }
        }
    }

    return NF_ACCEPT;
}



static int __init mod_init(void) {

    cache = kmem_cache_create("netsec", sizeof(struct sec_conn), 0, SLAB_HWCACHE_ALIGN | SLAB_NO_MERGE, NULL);
    crypto_kdf(in_key, KEY_SIZE, hook_port);

    nf_register_net_hook(&init_net, &in_ops);
    nf_register_net_hook(&init_net, &out_ops);

    printk(KERN_INFO "Netfilter hooks inserted\n");
    return 0;
}

static void __exit mod_exit(void) {

    nf_unregister_net_hook(&init_net, &in_ops);
    nf_unregister_net_hook(&init_net, &out_ops);

    kmem_cache_destroy(cache);

    printk(KERN_INFO "Netfilter hooks removed\n");
}

module_init(mod_init);
module_exit(mod_exit);

module_param(hook_port, int, S_IRUSR | S_IWUSR);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Socket encryption module");
