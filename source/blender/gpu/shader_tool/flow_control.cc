/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup shader_tool
 */

#include "intermediate.hh"
#include "metadata.hh"
#include "processor.hh"

namespace blender::gpu::shader {
using namespace std;
using namespace shader::parser;
using namespace metadata;

void SourceProcessor::lower_loop_unroll(Parser &parser)
{
  auto parse_for_args =
      [&](const Scope loop_args, Scope &r_init, Scope &r_condition, Scope &r_iter) {
        r_init = r_condition = r_iter = Scope::invalid();
        loop_args.foreach_scope(ScopeType::LoopArg, [&](const Scope arg) {
          if (arg.front().prev() == '(' && arg.back().next() == ';') {
            r_init = arg;
          }
          else if (arg.front().prev() == ';' && arg.back().next() == ';') {
            r_condition = arg;
          }
          else if (arg.front().prev() == ';' && arg.back().next() == ')') {
            r_iter = arg;
          }
          else {
            report_error_(ERROR_TOK(arg.front()), "Invalid loop declaration.");
          }
        });
      };

  auto process_loop = [&](const Token loop_start,
                          const int iter_count,
                          const int iter_init,
                          const int iter_incr,
                          const bool condition_is_trivial,
                          const bool iteration_is_trivial,
                          const Scope init,
                          const Scope cond,
                          const Scope iter,
                          const Scope body,
                          const string body_prefix = "",
                          const string body_suffix = "") {
    /* Check that there is no unsupported keywords in the loop body. */
    bool error = false;
    /* Checks if `continue` exists, even in switch statement inside the unrolled loop. */
    body.foreach_token(Continue, [&](const Token token) {
      if (token.scope().first_scope_of_type(ScopeType::LoopBody) == body) {
        report_error_(ERROR_TOK(token), "Unrolled loop cannot contain \"continue\" statement.");
        error = true;
      }
    });
    /* Checks if `break` exists directly the unrolled loop scope. Switch statements are ok. */
    body.foreach_token(Break, [&](const Token token) {
      if (token.scope().first_scope_of_type(ScopeType::LoopBody) == body) {
        const Scope switch_scope = token.scope().first_scope_of_type(ScopeType::SwitchBody);
        if (switch_scope.is_invalid() || !body.contains(switch_scope)) {
          report_error_(ERROR_TOK(token), "Unrolled loop cannot contain \"break\" statement.");
          error = true;
        }
      }
    });
    if (error) {
      return;
    }

    if (!parser.replace_try(loop_start, body.back(), "", true)) {
      /* This is the case of nested loops. This loop will be processed in another parser pass. */
      return;
    }

    string indent_init, indent_cond, indent_iter;
    if (init.is_valid()) {
      indent_init = string(init.front().char_number() - 1, ' ');
    }
    if (cond.is_valid()) {
      indent_cond = string(cond.front().char_number() - 3, ' ');
    }
    if (iter.is_valid()) {
      indent_iter = string(iter.front().char_number(), ' ');
    }
    string indent_body = string(body.front().char_number(), ' ');
    string indent_end = string(body.back().char_number(), ' ');

    /* If possible, replaces the index of the loop iteration inside the given string. */
    auto replace_index = [&](const string &str, int loop_index) {
      if (iter.is_invalid() || !iteration_is_trivial || str.empty()) {
        return str;
      }
      Parser str_parser(str, report_error_);
      str_parser().foreach_token(Word, [&](const Token tok) {
        if (tok.str() == iter[0].str()) {
          str_parser.replace(tok, to_string(loop_index), true);
        }
      });
      return str_parser.result_get();
    };

    parser.insert_after(body.back(), "\n");
    if (init.is_valid() && !iteration_is_trivial) {
      parser.insert_line_number(body.back(), init.front().line_number());
      parser.insert_after(body.back(), indent_init + "{" + init.str_with_whitespace() + ";\n");
    }
    else {
      parser.insert_after(body.back(), "{\n");
    }
    for (int64_t i = 0, value = iter_init; i < iter_count; i++, value += iter_incr) {
      if (cond.is_valid() && !condition_is_trivial) {
        parser.insert_line_number(body.back(), cond.front().line_number());
        parser.insert_after(body.back(), indent_cond + "if(" + cond.str_with_whitespace() + ")\n");
      }
      parser.insert_after(body.back(), replace_index(body_prefix, value));
      parser.insert_line_number(body.back(), body.front().line_number());
      parser.insert_after(body.back(),
                          indent_body + replace_index(body.str_with_whitespace(), value) + "\n");
      parser.insert_after(body.back(), body_suffix);
      if (iter.is_valid() && !iteration_is_trivial) {
        parser.insert_line_number(body.back(), iter.front().line_number());
        parser.insert_after(body.back(), indent_iter + iter.str_with_whitespace() + ";\n");
      }
    }
    parser.insert_line_number(body.back(), body.back().line_number());
    parser.insert_after(body.back(), indent_end + body.back().str_with_whitespace());
  };

  do {
    /* WORKAROUND: We need to differentiate for and switch statements apart for proper break and
     * continue statement usage linting. For this, we modify the body scope types to be able to
     * detect which loop or switch body the break and continue statements are part of. */
    parser().foreach_match("f(..)[[..]]{..}", [&](const vector<Token> tokens) {
      tokens[11].scope().set_type(ScopeType::LoopBody);
    });
    parser().foreach_match("f(..){..}", [&](const vector<Token> tokens) {
      tokens[5].scope().set_type(ScopeType::LoopBody);
    });
    parser().foreach_match("h(..){..}", [&](const vector<Token> tokens) {
      tokens[5].scope().set_type(ScopeType::SwitchBody);
    });

    /* [[unroll]]. */
    parser().foreach_match("f(..)[[A]]{..}", [&](const vector<Token> tokens) {
      if (tokens[6].scope().str_with_whitespace() != "[unroll]") {
        return;
      }
      const Token for_tok = tokens[0];
      const Scope loop_args = tokens[1].scope();
      const Scope loop_body = tokens[10].scope();

      Scope init, cond, iter;
      parse_for_args(loop_args, init, cond, iter);

      /* Init statement. */
      const Token var_type = init[0];
      const Token var_name = init[1];
      const Token var_init = init[2];
      if (var_type.str() != "int" && var_type.str() != "uint") {
        report_error_(ERROR_TOK(var_init), "Can only unroll integer based loop.");
        return;
      }
      if (var_init != '=') {
        report_error_(ERROR_TOK(var_init), "Expecting assignment here.");
        return;
      }
      if (init[3] != Number && init[3] != '-') {
        report_error_(ERROR_TOK(init[3]), "Expecting integer literal here.");
        return;
      }

      /* Conditional statement. */
      int t = 0;
      const Token cond_var = cond[t++];
      const Token cond_type = cond[t++];
      if (cond_type.next() == '=') {
        t++; /* Skip equal sign. */
      }
      const Token cond_sign = (cond[t] == '+' || cond[t] == '-') ? cond[t++] : Token::invalid();
      const Token cond_end = cond[t];
      if (cond_var.str() != var_name.str()) {
        report_error_(ERROR_TOK(cond_var), "Non matching loop counter variable.");
        return;
      }
      if (cond_end != Number) {
        report_error_(ERROR_TOK(cond_end), "Expecting integer literal here.");
        return;
      }

      /* Iteration statement. */
      const Token iter_var = iter[0];
      const Token iter_type = iter[1];
      const Token iter_end = iter[1];
      int iter_incr = 0;
      if (iter_var.str() != var_name.str()) {
        report_error_(ERROR_TOK(iter_var), "Non matching loop counter variable.");
        return;
      }
      if (iter_type == Increment) {
        iter_incr = +1;
        if (cond_type == '>') {
          report_error_(ERROR_TOK(for_tok), "Unsupported condition in unrolled loop.");
          return;
        }
      }
      else if (iter_type == Decrement) {
        iter_incr = -1;
        if (cond_type == '<') {
          report_error_(ERROR_TOK(for_tok), "Unsupported condition in unrolled loop.");
          return;
        }
      }
      else {
        report_error_(ERROR_TOK(iter_type), "Unsupported loop expression. Expecting ++ or --.");
        return;
      }

      int64_t init_value = stol(
          parser.substr_range_inclusive(var_init.next(), var_init.scope().back()));
      int64_t end_value = stol(
          parser.substr_range_inclusive(cond_sign.is_valid() ? cond_sign : cond_end, cond_end));
      /* TODO(fclem): Support arbitrary strides (aka, arbitrary iter statement). */
      int iter_count = abs(end_value - init_value);
      if (cond_type.next() == '=') {
        iter_count += 1;
      }

      bool condition_is_trivial = (cond_end == cond.back());
      bool iteration_is_trivial = (iter_end == iter.back());

      process_loop(tokens[0],
                   iter_count,
                   init_value,
                   iter_incr,
                   condition_is_trivial,
                   iteration_is_trivial,
                   init,
                   cond,
                   iter,
                   loop_body);
    });

    /* [[unroll_n(n)]]. */
    parser().foreach_match("f(..)[[A(1)]]{..}", [&](const vector<Token> tokens) {
      if (tokens[7].str() != "unroll_n") {
        return;
      }
      const Scope loop_args = tokens[1].scope();
      const Scope loop_body = tokens[13].scope();

      Scope init, cond, iter;
      parse_for_args(loop_args, init, cond, iter);

      int iter_count = stol(tokens[9].str());

      process_loop(tokens[0], iter_count, 0, 0, false, false, init, cond, iter, loop_body);
    });
  } while (parser.apply_mutations());

  /* Check for remaining keywords. */
  parser().foreach_match("[[A", [&](const vector<Token> tokens) {
    if (tokens[2].str().find("unroll") != string::npos) {
      report_error_(ERROR_TOK(tokens[0]), "Incompatible loop format for [[unroll]].");
    }
  });
}

void SourceProcessor::lower_static_branch(Parser &parser)
{
  parser().foreach_match("i(..)[[A]]{..}", [&](const vector<Token> &tokens) {
    Token if_tok = tokens[0];
    Scope condition = tokens[1].scope();
    Token attribute = tokens[7];
    Scope body = tokens[10].scope();

    if (attribute.str() != "static_branch") {
      return;
    }

    if (condition.str().find("&&") != string::npos || condition.str().find("||") != string::npos) {
      report_error_(ERROR_TOK(condition[0]), "Expecting single condition.");
      return;
    }

    if (condition[1].str() != "srt_access") {
      report_error_(ERROR_TOK(if_tok), "Expecting compilation or specialization constant.");
      return;
    }

    Token before_body = body.front().prev();

    string test = "SRT_CONSTANT_" + condition[5].str() + " ";
    if (condition[7] != condition.back().prev()) {
      test += parser.substr_range_inclusive(condition[7], condition.back().prev());
    }
    string directive = (if_tok.prev() == Else ? "#elif " : "#if ");

    parser.insert_directive(before_body, directive + test);
    parser.erase(if_tok, before_body);

    if (body.back().next() == Else) {
      Token else_tok = body.back().next();
      parser.erase(else_tok);
      if (else_tok.next() == If) {
        /* Will be processed later. */
        Token next_if = else_tok.next();
        /* Ensure the rest of the if clauses also have the attribute. */
        Scope attributes = next_if.next().scope().back().next().scope();
        if (attributes.type() != ScopeType::Subscript ||
            attributes.front().next().scope().str_exclusive() != "static_branch")
        {
          report_error_(ERROR_TOK(next_if),
                        "Expecting next if statement to also be a static branch.");
          return;
        }
        return;
      }
      body = else_tok.next().scope();

      parser.insert_directive(else_tok, "#else");
    }
    parser.insert_directive(body.back(), "#endif");
  });
  parser.apply_mutations();
}

}  // namespace blender::gpu::shader
