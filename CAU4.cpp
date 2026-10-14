#include <iostream>
#include <vector>
#include <queue>
#include <algorithm>

using namespace std;

const long long INF = 1e18;

struct Edge {
    int to;
    long long weight;
};

int main() {
    ios_base::sync_with_stdio(false);
    cin.tie(NULL);

    if (freopen("CAU4.INP", "r", stdin)) {}
    if (freopen("CAU4.OUT", "w", stdout)) {}

    int n, m;
    if (!(cin >> n >> m)) return 0;

    vector<vector<Edge>> adj(n + 1);

    // Free backward edges: j -> j - 1 with weight 0
    for (int j = 1; j <= n; j++) {
        adj[j].push_back({j - 1, 0});
    }

    // Artist edges: s_i -> t_i with weight c_i
    for (int i = 0; i < m; i++) {
        int s, t;
        long long c;
        cin >> s >> t >> c;
        adj[s].push_back({t, c});
    }

    // Dijkstra algorithm from 0 to n
    vector<long long> dist(n + 1, INF);
    priority_queue<pair<long long, int>, vector<pair<long long, int>>, greater<pair<long long, int>>> pq;

    dist[0] = 0;
    pq.push({0, 0});

    while (!pq.empty()) {
        auto [d, u] = pq.top();
        pq.pop();

        if (d > dist[u]) continue;

        for (const auto& edge : adj[u]) {
            if (dist[u] + edge.weight < dist[edge.to]) {
                dist[edge.to] = dist[u] + edge.weight;
                pq.push({dist[edge.to], edge.to});
            }
        }
    }

    cout << dist[n] << "\n";

    return 0;
}
