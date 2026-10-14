#include <iostream>
#include <string>
#include <vector>
#include <algorithm>

using namespace std;

string add(string a, int b)
{
    int i = a.size() - 1, c = b;
    while (i >= 0 && c)
    {
        int x = a[i] - '0' + c;
        a[i] = x % 10 + '0';
        c = x / 10;
        i--;
    }
    while (c)
    {
        a = char(c % 10 + '0') + a;
        c /= 10;
    }
    return a;
}

string sub(string a, int b)
{
    int i = a.size() - 1;
    while (i >= 0 && b)
    {
        int x = a[i] - '0' - b % 10;
        b /= 10;
        if (x < 0)
        {
            x += 10;
            b++;
        }
        a[i] = x + '0';
        i--;
    }
    while (a.size() > 1 && a[0] == '0') a.erase(0, 1);
    return a;
}

string mul(string a, string b)
{
    if (a == "0" || b == "0") return "0";
    int n = a.size(), m = b.size();
    vector<int> c(n + m, 0);
    for (int i = n - 1; i >= 0; i--)
    {
        int carry = 0;
        for (int j = m - 1; j >= 0; j--)
        {
            int x = (a[i] - '0') * (b[j] - '0') + c[i + j + 1] + carry;
            c[i + j + 1] = x % 10;
            carry = x / 10;
        }
        c[i] = carry;
    }
    string s = "";
    int i = 0;
    while (i < (int)c.size() && c[i] == 0) i++;
    for (; i < (int)c.size(); i++) s += char(c[i] + '0');
    return s;
}

string div4(string a)
{
    string s = "";
    int r = 0;
    for (char c : a)
    {
        int x = r * 10 + c - '0';
        s += char(x / 4 + '0');
        r = x % 4;
    }
    while (s.size() > 1 && s[0] == '0') s.erase(0, 1);
    return s;
}

int main()
{
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    if (freopen("CAU1.INP", "r", stdin)) {}
    if (freopen("CAU1.OUT", "w", stdout)) {}

    string n;
    if (cin >> n) {
        string a = sub(n, 1);
        string b = n;
        string c = add(n, 1);
        string d = add(n, 2);
        cout << div4(mul(mul(a, b), mul(c, d))) << "\n";
    }
    return 0;
}
