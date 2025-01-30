#define let auto const
#define var auto

void check_error(char const *file, int line);
#define ce check_error(__FILE__, __LINE__)

void shaderReport_(char const *file, int line, int shader, char const *name = "whatever");
void programReport_(char const *file, int line, int prog, char const *name = "whatever");

#define shaderReport(...) shaderReport_(__FILE__, __LINE__, __VA_ARGS__)
#define programReport(...) programReport_(__FILE__, __LINE__, __VA_ARGS__)


struct Tag {
    char const *name;
    char const *content_beg;
    char const *content_end;
    int name_c;
    int children_e;
};

struct Tags {
    Tag *tags;
    int count;
};

Tags htmlToTags(char *data, int len);
