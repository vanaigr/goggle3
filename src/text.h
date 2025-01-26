struct FontDef {
    int font_size;
    bool bold;


};

inline bool operator==(FontDef a, FontDef b) {
    return a.font_size == b.font_size && a.bold == b.bold;
}

int text_init();
void text_bind_texture(int texture);
void text_draw(char const *text, int text_c, FontDef fd, int x, int y, int max_width);
