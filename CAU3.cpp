#include <iostream>
#include <vector>

using namespace std;

static const int MAXN = 100005;

long long a[MAXN];
long long tree[4 * MAXN];
long long lazy[4 * MAXN];

void build(int node, int start, int end) {
    if (start == end) {
        tree[node] = a[start];
        lazy[node] = 0;
        return;
    }
    int mid = (start + end) / 2;
    build(2 * node, start, mid);
    build(2 * node + 1, mid + 1, end);
    tree[node] = tree[2 * node] + tree[2 * node + 1];
    lazy[node] = 0;
}

void push(int node, int start, int end) {
    if (lazy[node] != 0) {
        int mid = (start + end) / 2;

        lazy[2 * node] += lazy[node];
        tree[2 * node] += lazy[node] * (mid - start + 1);

        lazy[2 * node + 1] += lazy[node];
        tree[2 * node + 1] += lazy[node] * (end - mid);

        lazy[node] = 0;
    }
}

void update(int node, int start, int end, int l, int r, long long val) {
    if (r < start || end < l) return;
    if (l <= start && end <= r) {
        tree[node] += val * (end - start + 1);
        lazy[node] += val;
        return;
    }
    push(node, start, end);
    int mid = (start + end) / 2;
    update(2 * node, start, mid, l, r, val);
    update(2 * node + 1, mid + 1, end, l, r, val);
    tree[node] = tree[2 * node] + tree[2 * node + 1];
}

long long query(int node, int start, int end, int l, int r) {
    if (r < start || end < l) return 0;
    if (l <= start && end <= r) return tree[node];
    push(node, start, end);
    int mid = (start + end) / 2;
    return query(2 * node, start, mid, l, r) + query(2 * node + 1, mid + 1, end, l, r);
}

int main() {
    ios_base::sync_with_stdio(false);
    cin.tie(NULL);

    if (freopen("CAU3.INP", "r", stdin)) {}
    if (freopen("CAU3.OUT", "w", stdout)) {}

    int n, q;
    if (!(cin >> n >> q)) return 0;

    for (int i = 1; i <= n; i++) {
        cin >> a[i];
    }

    build(1, 1, n);

    while (q--) {
        int type;
        cin >> type;
        if (type == 1) {
            int p, m;
            long long x;
            cin >> p >> m >> x;
            update(1, 1, n, p, m, x);
        } else if (type == 2) {
            int u, v;
            cin >> u >> v;
            cout << query(1, 1, n, u, v) << "\n";
        }
    }

    return 0;
}
