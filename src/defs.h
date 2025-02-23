#define let auto const
#define var auto

void check_error(char const *file, int line);
#define ce check_error(__FILE__, __LINE__)

void shaderReport_(char const *file, int line, int shader, char const *name = "whatever");
void programReport_(char const *file, int line, int prog, char const *name = "whatever");

#define shaderReport(...) shaderReport_(__FILE__, __LINE__, __VA_ARGS__)
#define programReport(...) programReport_(__FILE__, __LINE__, __VA_ARGS__)


struct GLBuffer {
    int buffer;
    int count;
    int capacity;
};

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

#define P(a) (a).count, (a).items

struct Tags {
    Tag *items;
    int count;
    Attr *attrs; // malloc'ed
};

Tags htmlToTags(char const *data, int len);


struct Result {
    Tag const *title;
    Tag const *site_display_url;
    str rawUrl;
    str favicon_url;
    Tag const *desc;

    // TODO: date

    // str ping;
};

struct Results {
    Result *items;
    int count;
};

Results extractResults(Tags tags);

struct Texture {
    int offset; // -1 if none
    int width;
    int height;
};

struct FormattedStr {
    bool bold;
    bool italic;
    int len;
    char const *str;
    FormattedStr const *next;
};

struct PResult {
    str title;
    str site_display_url;
    str url;
    Texture texture;
    FormattedStr const *desc;
};

struct PResults {
    PResult *items;
    int count;
};

PResults processResults(Results res, GLBuffer &iconsBuf);

struct FontDef {
    int font_size;
    unsigned color;
};

struct Draw {
    int x;
    int y;
    int w;
    int h;
    int off;
};

struct DrawList {
    Draw *items;
    int count;
};

struct TextLayout {
    DrawList dl;
    int stop_x;
    int stop_y;
};

struct LayoutParams {
    int font_size;
    int max_width;
    int begin_x;
    int begin_y;
    int line_height;
    bool wordbreak = true;
};

int text_init();
void text_bind_texture(int texture);
TextLayout prepare(FormattedStr const *text, LayoutParams params);
void draw(DrawList dl, int color, int x, int y);
void rect(int x, int y, int w, int h, unsigned color);
struct vec4{ float r, g, b, a; };
void rect(int x, int y, int w, int h, vec4 color);

bool open_url(str url);

struct Cookie {
    str name;
    str value;
};

struct Cookies {
    Cookie *items;
    int count;
};

Cookie parseOneCookie(char const *&cur, char const *end);
Cookies getCookies(char const *buf, int count);
str encodeCookies(Cookies cookies);

int reserveBytes(GLBuffer &buf, int byte_count);

Texture decodeIcon(str string, GLBuffer &buf);

struct TextureDesc {
    int off;
    int x, y;
    int w, h;
};
struct TextureDescs {
    TextureDesc const *items;
    int count;
};

int image_init(GLBuffer *buf);
void image_draw(TextureDescs descs);

struct Bitmap {
    unsigned char *data;
    int width, height;
};
Bitmap decode_png_rgba(const unsigned char *png_data, int png_size);
