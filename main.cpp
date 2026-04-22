#include <iostream>
#include <vector>
#include <queue>
#include <unordered_set>
#include <string>
using namespace std;

namespace Grammar {

enum class TransitionType { Epsilon, a, b };

struct Transition {
  TransitionType type;
  int to;
  Transition(TransitionType type, int to) : type(type), to(to) {}
};

struct NFA {
  int start;
  unordered_set<int> ends;
  vector<vector<Transition>> transitions;

  NFA() : start(0) {}

  unordered_set<int> GetEpsilonClosure(unordered_set<int> states) const {
    unordered_set<int> closure;
    queue<int> q;
    for (int s : states) if (!closure.count(s)) { closure.insert(s); q.push(s); }
    while (!q.empty()) {
      int u = q.front(); q.pop();
      if (u < 0 || u >= (int)transitions.size()) continue;
      for (auto &tr : transitions[u]) {
        if (tr.type == TransitionType::Epsilon && !closure.count(tr.to)) {
          closure.insert(tr.to);
          q.push(tr.to);
        }
      }
    }
    return closure;
  }

  unordered_set<int> Advance(unordered_set<int> current_states, char character) const {
    unordered_set<int> cur = GetEpsilonClosure(current_states);
    unordered_set<int> next;
    TransitionType t = (character == 'a') ? TransitionType::a : TransitionType::b;
    for (int s : cur) {
      if (s < 0 || s >= (int)transitions.size()) continue;
      for (auto &tr : transitions[s]) {
        if (tr.type == t) next.insert(tr.to);
      }
    }
    return GetEpsilonClosure(next);
  }

  bool IsAccepted(int state) const { return ends.count(state); }
  int GetStart() const { return start; }
};

static NFA MakeSimple(const char &character) {
  NFA nfa;
  nfa.start = 0;
  nfa.ends = {1};
  nfa.transitions.assign(2, {});
  nfa.transitions[0].push_back({character == 'a' ? TransitionType::a : TransitionType::b, 1});
  return nfa;
}

static NFA MakeStar(const char &character) {
  NFA nfa;
  nfa.start = 0;
  nfa.ends = {0};
  nfa.transitions.assign(1, {});
  nfa.transitions[0].push_back({character == 'a' ? TransitionType::a : TransitionType::b, 0});
  return nfa;
}

static NFA MakePlus(const char &character) {
  // one or more occurrences
  NFA base = MakeSimple(character);
  // add loop on end state to itself
  TransitionType t = (character == 'a') ? TransitionType::a : TransitionType::b;
  if ((int)base.transitions.size() <= *base.ends.begin()) base.transitions.resize(*base.ends.begin()+1);
  base.transitions[*base.ends.begin()].push_back({t, *base.ends.begin()});
  return base;
}

static NFA MakeQuestion(const char &character) {
  // zero or one: epsilon from start to end, plus simple edge
  NFA nfa;
  nfa.start = 0;
  nfa.ends = {1};
  nfa.transitions.assign(2, {});
  TransitionType t = (character == 'a') ? TransitionType::a : TransitionType::b;
  nfa.transitions[0].push_back({t, 1});
  nfa.transitions[0].push_back({TransitionType::Epsilon, 1});
  return nfa;
}

static NFA Concatenate(const NFA &nfa1, const NFA &nfa2) {
  NFA res;
  int offset = (int)nfa1.transitions.size();
  res.start = nfa1.start;
  res.transitions = nfa1.transitions;
  // append nfa2 transitions with offset
  res.transitions.resize(offset + nfa2.transitions.size());
  for (int i = 0; i < (int)nfa2.transitions.size(); ++i) {
    for (auto tr : nfa2.transitions[i]) res.transitions[offset + i].push_back({tr.type, tr.to + offset});
  }
  // epsilon from each end of nfa1 to start of nfa2
  for (int e : nfa1.ends) res.transitions[e].push_back({TransitionType::Epsilon, nfa2.start + offset});
  // ends are nfa2 ends shifted
  for (int e : nfa2.ends) res.ends.insert(e + offset);
  return res;
}

static NFA Union(const NFA &nfa1, const NFA &nfa2) {
  // new start 0, epsilon to nfa1.start+1 and nfa2.start+1; reindex nfa1 and nfa2 by +1 and +1+|nfa1|
  NFA res;
  int size1 = nfa1.transitions.size();
  int size2 = nfa2.transitions.size();
  res.start = 0;
  res.transitions.assign(1 + size1 + size2, {});
  // epsilon from new start to each old start
  res.transitions[0].push_back({TransitionType::Epsilon, 1 + nfa1.start});
  res.transitions[0].push_back({TransitionType::Epsilon, 1 + size1 + nfa2.start});
  // copy transitions for nfa1
  for (int i = 0; i < size1; ++i) {
    for (auto tr : nfa1.transitions[i]) res.transitions[1 + i].push_back({tr.type, 1 + tr.to});
  }
  // copy transitions for nfa2
  for (int i = 0; i < size2; ++i) {
    for (auto tr : nfa2.transitions[i]) res.transitions[1 + size1 + i].push_back({tr.type, 1 + size1 + tr.to});
  }
  // ends are union of shifted ends
  for (int e : nfa1.ends) res.ends.insert(1 + e);
  for (int e : nfa2.ends) res.ends.insert(1 + size1 + e);
  return res;
}

class RegexChecker {
  NFA nfa;
public:
  bool Check(const string &str) const {
    unordered_set<int> states = {nfa.GetStart()};
    for (char c : str) {
      states = nfa.Advance(states, c);
      if (states.empty()) return false;
    }
    for (int s : states) if (nfa.IsAccepted(s)) return true;
    return false;
  }
  RegexChecker(const string &regex) {
    // Parse regex with lowest precedence |; no parentheses; postfix operators ?,*,+
    // We'll scan segments separated by '|', build NFA for each, then union all.
    vector<string> alts;
    {
      string cur;
      for (char ch : regex) {
        if (ch == '|') { alts.push_back(cur); cur.clear(); }
        else cur.push_back(ch);
      }
      alts.push_back(cur);
    }

    auto build_seq = [&](const string &seq) {
      bool first = true;
      NFA cur;
      for (size_t i = 0; i < seq.size(); ) {
        char c = seq[i];
        if (c != 'a' && c != 'b') { // invalid, treat as empty simple
          ++i; continue;
        }
        NFA base = MakeSimple(c);
        if (i + 1 < seq.size()) {
          char op = seq[i+1];
          if (op == '*') { base = MakeStar(c); i += 2; }
          else if (op == '+') { base = MakePlus(c); i += 2; }
          else if (op == '?') { base = MakeQuestion(c); i += 2; }
          else { i += 1; }
        } else { i += 1; }
        if (first) { cur = base; first = false; }
        else { cur = Concatenate(cur, base); }
      }
      if (first) { // empty sequence -> epsilon NFA that matches empty; but problem states empty string undefined; still build epsilon NFA to avoid crash
        NFA e; e.start = 0; e.ends = {0}; e.transitions.assign(1, {}); return e;
      }
      return cur;
    };

    NFA built = build_seq(alts[0]);
    for (size_t i = 1; i < alts.size(); ++i) built = Union(built, build_seq(alts[i]));
    nfa = built;
  }
};

} // namespace Grammar

int main() {
  ios::sync_with_stdio(false);
  cin.tie(nullptr);

  using namespace Grammar;
  string regex;
  if (!(cin >> regex)) return 0;
  RegexChecker checker(regex);
  // Try to read next token. If it's an integer count, read that many strings.
  string token;
  if (!(cin >> token)) return 0;
  auto is_number = [](const string &s){ if (s.empty()) return false; for (char c: s) if (!(c>='0'&&c<='9')) return false; return true; };
  vector<string> queries;
  if (is_number(token)) {
    int T = stoi(token);
    queries.reserve(T);
    for (int i = 0; i < T; ++i) { string s; if (!(cin >> s)) s = ""; queries.push_back(s); }
  } else {
    queries.push_back(token);
    string s;
    while (cin >> s) queries.push_back(s);
  }
  for (auto &s : queries) cout << (checker.Check(s) ? "Yes\n" : "No\n");
  return 0;
}
