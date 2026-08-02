#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <cbor.h>

#define MAX_PLAYERS     10

typedef struct {
    size_t len;
    char   buf[256];
} bytestring_t;

typedef struct {
    bytestring_t nickname;
    bytestring_t email;
    char        speciality[64];
} player_t;

typedef struct {
    uint64_t  year;
    char      captain[64];
    player_t  players[MAX_PLAYERS];
    size_t    player_count;
} team_t;

static cbor_item_t *find_value(cbor_item_t *map, const char *key)
{
    if (!cbor_isa_map(map))
        return NULL;

    size_t n = cbor_map_size(map);
    struct cbor_pair *pairs = cbor_map_handle(map);

    for (size_t i = 0; i < n; i++) {
        cbor_item_t *k = pairs[i].key;
        if (!cbor_isa_string(k) || !cbor_string_is_definite(k))
            continue;

        size_t klen = cbor_string_length(k);
        if (klen == strlen(key) &&
            memcmp(cbor_string_handle(k), key, klen) == 0)
            return pairs[i].value;
    }
    return NULL;
}

static ssize_t extract_string(cbor_item_t *map, const char *key,
                              char *dst, size_t dst_size)
{
    cbor_item_t *val = find_value(map, key);
    if (!val || !cbor_isa_string(val) || !cbor_string_is_definite(val))
        return -1;

    size_t len = cbor_string_length(val);
    size_t n   = len < dst_size - 1 ? len : dst_size - 1;
    memcpy(dst, cbor_string_handle(val), n);
    dst[n] = '\0';
    return (ssize_t)n;
}

static int extract_bytestring(cbor_item_t *map, const char *key,
                              bytestring_t *dst)
{
    cbor_item_t *val = find_value(map, key);
    if (!val || !cbor_isa_bytestring(val) || !cbor_bytestring_is_definite(val))
        return -1;

    dst->len = cbor_bytestring_length(val);
    size_t copy = dst->len < sizeof(dst->buf) - 1
                  ? dst->len : sizeof(dst->buf) - 1;
    memcpy(dst->buf, cbor_bytestring_handle(val), copy);
    dst->buf[copy] = '\0';
    return 0;
}

static int validate_email(bytestring_t *email)
{
    char user[32];
    char domain[32];

    const char *at = memchr(email->buf, '@', email->len);
    if (!at) {
        fprintf(stderr, "Error: invalid email format (missing '@')\n");
        return -1;
    }

    size_t user_len   = at - email->buf;
    size_t domain_len = email->len - user_len - 1;

    if ((user_len == 0 || user_len >= sizeof user) &&
        (domain_len == 0 || domain_len >= sizeof domain)) {
        fprintf(stderr, "Error: invalid email format (user or domain too long)\n");
        return -1;
    }

    memcpy(user, email->buf, user_len);
    user[user_len] = '\0';

    memcpy(domain, at + 1, domain_len);
    domain[domain_len] = '\0';

    if (strcmp(domain, "teamfrance.ctf") != 0) {
        fprintf(stderr, "Error: email domain must be 'teamfrance.ctf', got '%s'\n",
                domain);
        return -1;
    }

    return 0;
}

static void print_bytefield(const char *label, const char *buf, size_t len)
{
    printf("- %s: ", label);
    fwrite(buf, 1, len, stdout);
    printf("\n");
}

static void print_stringfield(const char *label, const char *buf)
{
    printf("- %s: %s\n", label, buf);
}


static int parse_player(cbor_item_t *player_map, player_t *player,
                        size_t index)
{
    if (!cbor_isa_map(player_map)) {
        fprintf(stderr, "Error: player #%zu is not a map\n", index);
        return -1;
    }

    if (extract_bytestring(player_map, "nickname", &player->nickname) < 0) {
        fprintf(stderr, "Error: missing or invalid field 'nickname' in player #%zu\n",
                index);
        return -1;
    }

    if (extract_bytestring(player_map, "email", &player->email) < 0) {
        fprintf(stderr, "Error: missing or invalid field 'email' in player #%zu\n",
                index);
        return -1;
    }

    if (validate_email(&player->email) < 0)
        return -1;

    if (extract_string(player_map, "speciality", player->speciality,
                       sizeof(player->speciality)) < 0) {
        fprintf(stderr, "Error: missing or invalid field 'speciality' in player #%zu\n",
                index);
        return -1;
    }

    return 0;
}

static int parse_team_info(cbor_item_t *map, team_t *team)
{
    cbor_item_t *year_val = find_value(map, "year");
    if (!year_val || !cbor_isa_uint(year_val)) {
        fprintf(stderr, "Error: missing or invalid field 'year'\n");
        return -1;
    }
    team->year = cbor_get_uint64(year_val);

    if (extract_string(map, "captain", team->captain,
                       sizeof(team->captain)) < 0) {
        fprintf(stderr, "Error: missing or invalid field 'captain'\n");
        return -1;
    }


    cbor_item_t *players_val = find_value(map, "players");
    if (!players_val || !cbor_isa_array(players_val)) {
        fprintf(stderr, "Error: missing or invalid field 'players'\n");
        return -1;
    }

    size_t count = cbor_array_size(players_val);
    team->player_count = count < MAX_PLAYERS ? count : MAX_PLAYERS;
    cbor_item_t **items = cbor_array_handle(players_val);

    for (size_t i = 0; i < team->player_count; i++) {
        if (parse_player(items[i], &team->players[i], i) < 0)
            return -1;
    }

    return 0;
}

static void print_team(const team_t *team)
{
    puts("Team registration info:\n");
    printf("Year: %lu\n", (unsigned long)team->year);
    printf("Captain: %s\n", team->captain);
    printf("Players: %zu player(s)\n", team->player_count);

    for (size_t i = 0; i < team->player_count; i++) {
        const player_t *p = &team->players[i];
        printf("Player #%zu:\n", i + 1);
        print_bytefield("Nickname", p->nickname.buf, p->nickname.len);
        print_bytefield("Email", p->email.buf, p->email.len);
        print_stringfield("Speciality", p->speciality);
        printf("--------------------------------\n");
    }
    printf("\n");
}

static void process_input(const unsigned char *buf, size_t len)
{
    struct cbor_load_result result;
    cbor_item_t *item = cbor_load(buf, len, &result);

    if (result.error.code != CBOR_ERR_NONE || !item) {
        fprintf(stderr, "Error: CBOR decoding failed (code %d)\n",
                result.error.code);
        if (item)
            cbor_decref(&item);

        return;
    }

    if (!cbor_isa_map(item)) {
        fprintf(stderr, "Error: invalid top level item\n");
        cbor_decref(&item);
        return;
    }

    team_t team = {};

    if (parse_team_info(item, &team) == 0)
        print_team(&team);

    cbor_decref(&item);
}

void __attribute__((constructor)) init(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stdin,  NULL, _IONBF, 0);
}

int main(void)
{
    static unsigned char buf[4096];

    puts("=== Team France Registration Form ===\n");

    while (1) {
        printf("Enter the team composition: ");

        ssize_t n = read(STDIN_FILENO, buf, sizeof(buf));
        if (n <= 0)
            break;

        process_input(buf, n);
        printf("\n");

        printf("Is it correct ? (y/n): ");
        char answer[2] = {0};
        ssize_t r = read(STDIN_FILENO, answer, sizeof(answer));
        if (r <= 0 || answer[0] == 'y')
            break;
    }

    puts("Your registration has been submitted successfully!");
    return 0;
}
