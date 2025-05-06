
float s, t;

void add_term(int i)
{
    float fi, delta;
    fi = (float) i;
    t = t * (2.0 * fi - 1.0);
    t = t / (8.0 * fi);

    delta = t / (2.0 * fi + 1.0);
    s = s + delta;

    putfloat(s);
    putchar(10);
}

int main()
{
    s = t = 3.0;

    add_term(1);
    add_term(2);
    add_term(3);
    add_term(4);
    add_term(5);
    add_term(6);
    add_term(7);
    add_term(8);
    return 0;
}
