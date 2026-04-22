#include "src.hpp"
#include <bits/stdc++.h>
using namespace std;
using namespace Grammar;

int main() {
  ios::sync_with_stdio(false);
  cin.tie(nullptr);

  string regex;
  if (!(cin >> regex)) return 0;
  RegexChecker checker(regex);
  string token;
  if (!(cin >> token)) return 0;
  auto is_number = [](const string &s){ if (s.empty()) return false; for (char c: s) if (!(c>='0'&&c<='9')) return false; return true; };
  vector<string> queries;
  if (is_number(token)) {
    int T = stoi(token);
    for (int i = 0; i < T; ++i) { string s; if (!(cin >> s)) s = ""; queries.push_back(s); }
  } else {
    queries.push_back(token);
    string s;
    while (cin >> s) queries.push_back(s);
  }
  for (auto &s : queries) cout << (checker.Check(s) ? "Yes\n" : "No\n");
  return 0;
}
