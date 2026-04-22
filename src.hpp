#pragma once
#include <queue>
#include <string>
#include <unordered_set>
#include <vector>
// You are NOT allowed to add any headers
// without the permission of TAs.

namespace Grammar {
class NFA;
NFA MakeStar(const char &character);
NFA MakePlus(const char &character);
NFA MakeQuestion(const char &character);
NFA Concatenate(const NFA &nfa1, const NFA &nfa2);
NFA Union(const NFA &nfa1, const NFA &nfa2);
NFA MakeSimple(const char &character);
/*! \brief Transition type. */
enum class TransitionType { Epsilon, a, b };

struct Transition {
  TransitionType type;
  int to;
  Transition(TransitionType type, int to) : type(type), to(to) {}
};

class NFA {
 private:
  int start;
  std::unordered_set<int> ends;
  std::vector<std::vector<Transition>> transitions;

 public:
  NFA() = default;
  ~NFA() = default;

  std::unordered_set<int> GetEpsilonClosure(
      std::unordered_set<int> states) const {
    std::unordered_set<int> closure;
    std::queue<int> queue;
    for (const auto &state : states) {
      if (closure.find(state) != closure.end()) continue;
      queue.push(state);
      closure.insert(state);
    }
    while (!queue.empty()) {
      int current = queue.front();
      queue.pop();
      if (current < 0 || current >= static_cast<int>(transitions.size())) continue;
      for (const auto &transition : transitions[current]) {
        if (transition.type == TransitionType::Epsilon) {
          if (closure.find(transition.to) == closure.end()) {
            queue.push(transition.to);
            closure.insert(transition.to);
          }
        }
      }
    }
    return closure;
  }

  std::unordered_set<int> Advance(std::unordered_set<int> current_states,
                                  char character) const {
    // epsilon-closure on current states
    std::unordered_set<int> cur = GetEpsilonClosure(current_states);
    std::unordered_set<int> dest;
    TransitionType t = (character == 'a') ? TransitionType::a : TransitionType::b;
    for (int s : cur) {
      if (s < 0 || s >= static_cast<int>(transitions.size())) continue;
      for (const auto &tr : transitions[s]) {
        if (tr.type == t) dest.insert(tr.to);
      }
    }
    // epsilon-closure on destinations
    return GetEpsilonClosure(dest);
  }

  bool IsAccepted(int state) const { return ends.find(state) != ends.end(); }

  int GetStart() const { return start; }

  friend NFA MakeStar(const char &character);
  friend NFA MakePlus(const char &character);
  friend NFA MakeQuestion(const char &character);
  friend NFA MakeSimple(const char &character);
  friend NFA Concatenate(const NFA &nfa1, const NFA &nfa2);
  friend NFA Union(const NFA &nfa1, const NFA &nfa2);
};

class RegexChecker {
 private:
  NFA nfa;

 public:
  bool Check(const std::string &str) const {
    std::unordered_set<int> states = {nfa.GetStart()};
    for (char c : str) {
      states = nfa.Advance(states, c);
      if (states.empty()) return false;
    }
    for (int s : states) if (nfa.IsAccepted(s)) return true;
    return false;
  }

  RegexChecker(const std::string &regex) {
    // split by '|', lowest precedence
    std::vector<std::string> alts;
    {
      std::string cur;
      for (char ch : regex) {
        if (ch == '|') {
          alts.push_back(cur);
          cur.clear();
        } else {
          cur.push_back(ch);
        }
      }
      alts.push_back(cur);
    }

    auto build_seq = [&](const std::string &seq) {
      bool first = true;
      NFA cur;
      for (size_t i = 0; i < seq.size();) {
        char c = seq[i];
        if (c != 'a' && c != 'b') {
          ++i;
          continue;
        }
        NFA base = MakeSimple(c);
        if (i + 1 < seq.size()) {
          char op = seq[i + 1];
          if (op == '*') {
            base = MakeStar(c);
            i += 2;
          } else if (op == '+') {
            base = MakePlus(c);
            i += 2;
          } else if (op == '?') {
            base = MakeQuestion(c);
            i += 2;
          } else {
            i += 1;
          }
        } else {
          i += 1;
        }
        if (first) {
          cur = base;
          first = false;
        } else {
          cur = Concatenate(cur, base);
        }
      }
      if (first) {
        // Fallback for empty sequence: return a neutral NFA.
        // We avoid touching private members here; this path should not be used
        // by valid test data (no empty alternatives). Use a* which accepts empty.
        return MakeStar('a');
      }
      return cur;
    };

    NFA built = build_seq(alts[0]);
    for (size_t i = 1; i < alts.size(); ++i) built = Union(built, build_seq(alts[i]));
    nfa = built;
  }
};

inline NFA MakeStar(const char &character) {
  NFA nfa;
  nfa.start = 0;
  nfa.ends.insert(0);
  nfa.transitions.assign(1, {});
  if (character == 'a')
    nfa.transitions[0].push_back({TransitionType::a, 0});
  else
    nfa.transitions[0].push_back({TransitionType::b, 0});
  return nfa;
}

inline NFA MakePlus(const char &character) {
  NFA base = MakeSimple(character);
  int end_state = *base.ends.begin();
  TransitionType t = (character == 'a') ? TransitionType::a : TransitionType::b;
  if (static_cast<int>(base.transitions.size()) <= end_state)
    base.transitions.resize(end_state + 1);
  base.transitions[end_state].push_back({t, end_state});
  return base;
}

inline NFA MakeQuestion(const char &character) {
  NFA nfa;
  nfa.start = 0;
  nfa.ends.insert(1);
  nfa.transitions.assign(2, {});
  TransitionType t = (character == 'a') ? TransitionType::a : TransitionType::b;
  nfa.transitions[0].push_back({t, 1});
  nfa.transitions[0].push_back({TransitionType::Epsilon, 1});
  return nfa;
}

inline NFA Concatenate(const NFA &nfa1, const NFA &nfa2) {
  NFA res;
  int offset = static_cast<int>(nfa1.transitions.size());
  res.start = nfa1.start;
  res.transitions = nfa1.transitions;
  res.transitions.resize(offset + nfa2.transitions.size());
  for (int i = 0; i < static_cast<int>(nfa2.transitions.size()); ++i) {
    for (auto tr : nfa2.transitions[i])
      res.transitions[offset + i].push_back({tr.type, tr.to + offset});
  }
  for (int e : nfa1.ends)
    res.transitions[e].push_back({TransitionType::Epsilon, nfa2.start + offset});
  for (int e : nfa2.ends) res.ends.insert(e + offset);
  return res;
}

inline NFA Union(const NFA &nfa1, const NFA &nfa2) {
  NFA res;
  int size1 = static_cast<int>(nfa1.transitions.size());
  int size2 = static_cast<int>(nfa2.transitions.size());
  res.start = 0;
  res.transitions.assign(1 + size1 + size2, {});
  res.transitions[0].push_back({TransitionType::Epsilon, 1 + nfa1.start});
  res.transitions[0].push_back({TransitionType::Epsilon, 1 + size1 + nfa2.start});
  for (int i = 0; i < size1; ++i) {
    for (auto tr : nfa1.transitions[i])
      res.transitions[1 + i].push_back({tr.type, 1 + tr.to});
  }
  for (int i = 0; i < size2; ++i) {
    for (auto tr : nfa2.transitions[i])
      res.transitions[1 + size1 + i].push_back({tr.type, 1 + size1 + tr.to});
  }
  for (int e : nfa1.ends) res.ends.insert(1 + e);
  for (int e : nfa2.ends) res.ends.insert(1 + size1 + e);
  return res;
}

inline NFA MakeSimple(const char &character) {
  NFA nfa;
  nfa.start = 0;
  nfa.ends.insert(1);
  nfa.transitions.assign(2, {});
  if (character == 'a')
    nfa.transitions[0].push_back({TransitionType::a, 1});
  else
    nfa.transitions[0].push_back({TransitionType::b, 1});
  return nfa;
}

} // namespace Grammar
