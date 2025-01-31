#define let auto const
#define var auto

void check_error(char const *file, int line);
#define ce check_error(__FILE__, __LINE__)

void shaderReport_(char const *file, int line, int shader, char const *name = "whatever");
void programReport_(char const *file, int line, int prog, char const *name = "whatever");

#define shaderReport(...) shaderReport_(__FILE__, __LINE__, __VA_ARGS__)
#define programReport(...) programReport_(__FILE__, __LINE__, __VA_ARGS__)


struct str {
    char const *items;
    int count;
};

bool streq(str a, str b);

#define STR(a) (str{ .items = a, .count = sizeof(a) - 1 })
static inline str mkstr(char const *b, char const *e) {
    return { b, (int)(e - b) };
}

char const *find(char const *b, char const *e, char c);

struct Attr {
    str name;
    str value;
};

struct Tag {
    char const *begin;
    str name;
    char const *content_beg;
    char const *content_end;
    char const *end;
    Tag *descendants_e;
    int attrs_beg;
    int attrs_end;
};

struct Tags {
    Tag *items;
    int count;
    Attr *attrs; // malloc'ed
};

Tags htmlToTags(char const *data, int len);


struct Result {
    Tag const *title;
    Tag const *site_name;
    Tag const *site_display_url;
    str rawUrl;
    Tag const *desc;

    // str ping;
    // str favicon;
};

struct Results {
    Result *items;
    int count;
};

Results extractResults(Tags tags);

struct FormattedStr {
    bool bold;
    bool italic;
    int len;
    FormattedStr const *next;
    char str[];
};

struct PResult {
    str title;
    str site_name;
    str site_display_url;
    str url;
    FormattedStr const *desc;
};

struct PResults {
    PResult *items;
    int count;
};

PResults processResults(Results res);
