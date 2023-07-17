#include <concord/discord.h>
#include <concord/log.h>
#include <dotenv.h>
#include <pcre.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

pcre *REGEX;

void on_ready(struct discord *client, const struct discord_ready *event) {
    log_info("%s#%s connected!", event->user->username,
             event->user->discriminator);
}

void on_message(struct discord *client, const struct discord_message *event) {
    if (event->author->bot) {
        return;
    }

    const char *msg_content = event->content;
    int ovector[30];

    int offset = 0;
    while (offset <= strlen(msg_content)) {
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
            snprintf(buf, sizeof(buf), "https://github.com/%s/%s/issues/%s",
                     username, repo, issue_num);

            struct discord_create_message params = {.content = buf};
            discord_create_message(client, event->channel_id, &params, NULL);
        }

        if (rc == PCRE_ERROR_NOMATCH) {
            break;
        }
    }

    if (strcmp(msg_content, "ping") == 0) {
        struct discord_create_message params = {.content = "pong"};
        discord_create_message(client, event->channel_id, &params, NULL);
    }
}

int main() {
    env_load(".", false);
    char *bot_token = getenv("DISCORD_TOKEN");

    const char *error;
    int erroffset;

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

    return 0;
}