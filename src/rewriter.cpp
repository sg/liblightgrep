
#include "rewriter.h"

#include <iostream>
#include <stack>

#include <boost/lexical_cast.hpp>

void print_tree(std::ostream& out, const Node& n) {
  if (n.Left) {
    // this node has a left child
    print_tree(out, *n.Left);
  }

  if (n.Right) {
    // this node has a right child
    print_tree(out, *n.Right);
  }

  out << n << '\n';
}

void print_branch(std::ostream& out, std::stack<Node*>& branch) {
  std::stack<Node*> tmp;

  while (!branch.empty()) {
    tmp.push(branch.top());
    branch.pop();
  }

  Node* n;
  while (!tmp.empty()) {
    n = tmp.top();
    out << *n << '\n';
    branch.push(n);
    tmp.pop();
  }
}

bool has_zero_length_match(const Node *n) {
  switch (n->Type) {
  case Node::REGEXP:
  case Node::PLUS:
  case Node::PLUS_NG:
    return has_zero_length_match(n->Left);  

  case Node::STAR:
  case Node::QUESTION:
  case Node::STAR_NG:
  case Node::QUESTION_NG:
    return true;

  case Node::ALTERNATION:
    return has_zero_length_match(n->Left) || has_zero_length_match(n->Right);

  case Node::CONCATENATION:
    return has_zero_length_match(n->Left) && has_zero_length_match(n->Right);

  case Node::DOT:
  case Node::CHAR_CLASS:
  case Node::LITERAL:
    return false;

  default:
    // WTF?
    throw std::logic_error(boost::lexical_cast<std::string>(n->Type));
  }
}

bool prefers_zero_length_match(const Node* n) {
  switch (n->Type) {
  case Node::REGEXP:
  case Node::PLUS:
  case Node::PLUS_NG:
  case Node::STAR:
  case Node::QUESTION:
    return prefers_zero_length_match(n->Left);

  case Node::STAR_NG:
  case Node::QUESTION_NG:
    return true;

  case Node::ALTERNATION:
    return prefers_zero_length_match(n->Left) ||
           prefers_zero_length_match(n->Right);

  case Node::CONCATENATION:
    return prefers_zero_length_match(n->Left) &&
           prefers_zero_length_match(n->Right);

  case Node::DOT:
  case Node::CHAR_CLASS:
  case Node::LITERAL:
    return false;

  default:
    // WTF?
    throw std::logic_error(boost::lexical_cast<std::string>(n->Type));
  }
}

bool reduce_trailing_nongreedy_then_empty(Node *root) {
  // Look for S+?T along trailing branches, where T admits zero-length
  // matches.

  Node* n = root;
  while (n) {
    switch (n->Type) {
    case Node::REGEXP:
    case Node::PLUS:
    case Node::STAR:
    case Node::QUESTION:
    case Node::ALTERNATION:
    case Node::PLUS_NG:
    case Node::STAR_NG:
    case Node::QUESTION_NG:
      // these are not the nodes you're looking for
      break;

    case Node::CONCATENATION:
      if (n->Left->Type == Node::PLUS_NG && has_zero_length_match(n->Right)) {
        n->Left = n->Left->Left;
        return true;
      }
      break;

    case Node::DOT:
    case Node::CHAR_CLASS:
    case Node::LITERAL:
      // branch finished
      break;
    default:
      // WTF?
      throw std::logic_error(boost::lexical_cast<std::string>(n->Type));
    }

    n = n->Right ? n->Right : n->Left;
  }

  return false;
}

void prune_subtree(Node *n, std::stack<Node*>& branch) {
  if (n->Type == Node::REGEXP) {
    n->Left = 0;
    branch.push(n);
    return;
  }

  Node* p = branch.top();

  switch (p->Type) {
  case Node::REGEXP:
    // the whole pattern has been pruned!
    p->Left = 0;
    break;

  case Node::ALTERNATION:
  case Node::CONCATENATION:
    // replace the parent node with the sibling node
    {
      Node* sibling = p->Left == n ? p->Right : p->Left;

      branch.pop();
      Node* gp = branch.top();

      if (gp->Left == p) {
        gp->Left = sibling;
      }
      else {
        gp->Right = sibling;
      }
    }
    break;

  case Node::PLUS:
  case Node::STAR:
  case Node::QUESTION:
  case Node::PLUS_NG:
  case Node::STAR_NG:
  case Node::QUESTION_NG:
    // prune the parent node
    branch.pop();
    prune_subtree(p, branch);
    break;

  default:
    // WTF?
    throw std::logic_error(boost::lexical_cast<std::string>(n->Type));
  }
}

bool reduce_trailing_nongreedy(Node* n, std::stack<Node*>& branch) {
  switch (n->Type) {
  case Node::REGEXP:
    if (!n->Left) {
      return false;
    }
  case Node::PLUS:
  case Node::STAR:
  case Node::QUESTION:
    branch.push(n);
    return reduce_trailing_nongreedy(n->Left, branch);

  case Node::CONCATENATION:
    branch.push(n);
    return reduce_trailing_nongreedy(n->Right, branch);

  case Node::ALTERNATION:
    {
      branch.push(n);
      std::stack<Node*> orig_branch(branch);

      const bool lreduce = reduce_trailing_nongreedy(n->Left, branch);

      if (n->Left) {
        // The left alternative wasn't pruned away, so we have to traverse
        // the subtree of the right alternative ourselves.
        return reduce_trailing_nongreedy(n->Right, orig_branch) || lreduce;
      }
      else {
        // The left alternative was pruned away, so the right alternative
        // was moved up the tree and has already been traversed.
        return lreduce;
      }
    }

  case Node::PLUS_NG:
    // strip out +?
    {
      Node* p = branch.top();

      if (p->Left == n) {
        p->Left = n->Left;
      }
      else {
        p->Right = n->Left;
      }

      reduce_trailing_nongreedy(n->Left, branch);
      return true;
    }

  case Node::STAR_NG:
  case Node::QUESTION_NG:
    // prune this subtree
    prune_subtree(n, branch);
    n = branch.top();
    branch.pop();
    reduce_trailing_nongreedy(n, branch);
    return true;

  case Node::DOT:
  case Node::CHAR_CLASS:
  case Node::LITERAL:
    // branch finished
    return false;

  default:
    // WTF?
    throw std::logic_error(boost::lexical_cast<std::string>(n->Type));
  }
}

bool reduce_trailing_nongreedy(Node* root) {
  std::stack<Node*> branch;
  return reduce_trailing_nongreedy(root, branch);
}
