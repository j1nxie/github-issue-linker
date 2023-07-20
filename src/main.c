#include "../dependencies/hashmap.c/hashmap.h"
#include <concord/discord.h>
#include <concord/log.h>
#include <dotenv.h>
#include <math.h>
#include <pcre.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

pcre *REGEX;
struct hashmap *MAP;

struct user {
    char *id;
    u64unix_ms timestamp;
} user;

int user_compare(const void *a, const void *b, void *udata) {
    const struct user *ua = a;
    const struct user *ub = b;
    return strcmp(ua->id, ub->id);
}

uint64_t user_hash(const void *item, uint64_t seed0, uint64_t seed1) {
    const struct user *user = item;
    return hashmap_sip(user->id, strlen(user->id), seed0, seed1);
}

void on_ready(struct discord *client, const struct discord_ready *event) {
    log_info("%s#%s connected!", event->user->username,
             event->user->discriminator);
}

void on_message(struct discord *client, const struct discord_message *event) {
    if (event->author->bot) {
        return;
    }

    u64snowflake id = event->author->id;
    int id_str_size = (ceil(log10(id)) + 1) * sizeof(char);
    char id_str[id_str_size];
    snprintf(id_str, id_str_size, "%zu", id);

    const char *msg_content = event->content;
    int ovector[30];
    char msg[1024] = "";

    int offset = 0;
    const struct user *user;
    user = hashmap_get(MAP, &(struct user){.id = id_str});
    if (user == NULL) {
        hashmap_set(
            MAP, &(struct user){.id = id_str, .timestamp = event->timestamp});
    } else {
        if (strcmp(user->id, id_str) == 0 &&
            event->timestamp - user->timestamp <= 3000) {
            return;
        }
    }
    while (offset <= strlen(event->content)) {
        int rc = pcre_exec(REGEX, NULL, msg_content, strlen(msg_content),
                           offset, 0, ovector, 30);
        if ((rc > -1)) {
            const char *username;
            const char *repo;
            const char *issue_num;
            (void)pcre_get_substring(msg_content, ovector, rc, 1, &username);
            (void)pcre_get_substring(msg_content, ovector, rc, 2, &repo);
            (void)pcre_get_substring(msg_content, ovector, rc, 3, &issue_num);

            offset = ovector[1];

            char buf[256];
            snprintf(buf, sizeof(buf), "https://github.com/%s/%s/issues/%s\n",
                     username, repo, issue_num);
            strcat(msg, buf);
        }

        if (rc == PCRE_ERROR_NOMATCH) {
            break;
        }
    }

    hashmap_set(MAP,
                &(struct user){.id = id_str, .timestamp = event->timestamp});
    struct discord_create_message params = {.content = msg};
    discord_create_message(client, event->channel_id, &params, NULL);
}

int main() {
    env_load(".", false);
    char *bot_token = getenv("DISCORD_TOKEN");

    const char *error;
    int erroffset;
    MAP =
        hashmap_new(sizeof(user), 0, 0, 0, user_hash, user_compare, NULL, NULL);

    REGEX = pcre_compile(
        "(?P<username>[a-z\\d](?:[a-z\\d]|-(?=[a-z\\d])){0,38})/(?P<"
        "repo>[a-zA-Z0-9-_]{0,100})#(?P<issue>\\d+)",
        PCRE_MULTILINE, &error, &erroffset, NULL);

    if (REGEX == NULL) {
        printf("PCRE compilation failed at offset %d: %s\n", erroffset, error);
        return 1;
    }

    struct discord *client = discord_init(bot_token);
    discord_add_intents(client, DISCORD_GATEWAY_MESSAGE_CONTENT);
    discord_set_on_ready(client, &on_ready);
    discord_set_on_message_create(client, &on_message);
    discord_run(client);

    pcre_free(REGEX);
    hashmap_free(MAP);
    return 0;
}