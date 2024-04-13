in f32 height;
out V4 FragColor;

void main()
{
    f32 h = (height + 30.f)/20.f;
    FragColor = V4(h, h, h, 1.f);
}
