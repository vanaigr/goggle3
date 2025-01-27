struct FontDef {
    int font_size;
    bool bold;
    unsigned color;
};

int text_init();
void text_bind_texture(int texture);
void text_draw(char const *text, int text_c, FontDef fd, int x, int y, int max_width);
