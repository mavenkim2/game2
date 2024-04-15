in f32 height;
out V4 FragColor;

void main()
{
    f32 h = (height + 30.f)/64.f;
    f32 blue = h;
    if (h <= 0.01)
    {
        blue = .5f;
    }
    FragColor = V4(h, h, blue, 1);

}
