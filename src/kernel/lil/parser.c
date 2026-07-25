#include "ll.h"

ASTNode *parse_expr(void);
ASTNode *parse_stmt(void);
ASTNode *parse_block(void);
ASTNode *parse_else_chain(void);
ASTNode *parse_postfix(void);

ASTNode *ast_alloc(NodeType type) {
    ASTNode *n = safe_alloc(sizeof(ASTNode));
    n->type = type;
    n->line = lex_cur.line;
    return n;
}

ASTNode *ast_num(double v) {
    ASTNode *n = ast_alloc(NODE_NUM);
    n->data.num = v;
    return n;
}

ASTNode *ast_str(const char *v) {
    ASTNode *n = ast_alloc(NODE_STR);
    n->data.str = sdup(v);
    return n;
}

ASTNode *ast_id(const char *name) {
    ASTNode *n = ast_alloc(NODE_ID);
    n->data.id = sdup(name);
    return n;
}

ASTNode *ast_binop(int op, ASTNode *l, ASTNode *r) {
    ASTNode *n = ast_alloc(NODE_BINOP);
    n->data.binop.op = op;
    n->data.binop.left = l;
    n->data.binop.right = r;
    return n;
}

ASTNode *ast_unary(int op, ASTNode *o) {
    ASTNode *n = ast_alloc(NODE_UNARY);
    n->data.unary.op = op;
    n->data.unary.operand = o;
    return n;
}

ASTNode *ast_assign(const char *name, ASTNode *val) {
    ASTNode *n = ast_alloc(NODE_ASSIGN);
    n->data.assign.name = sdup(name);
    n->data.assign.value = val;
    n->data.assign.typed = NULL;
    return n;
}

ASTNode *ast_print(ASTNode **exprs, int count) {
    ASTNode *n = ast_alloc(NODE_PRINT);
    n->data.print.exprs = exprs;
    n->data.print.count = count;
    n->data.print.cap = count;
    return n;
}

ASTNode *ast_input(const char *name, const char *prompt) {
    ASTNode *n = ast_alloc(NODE_INPUT);
    n->data.input.name = sdup(name);
    n->data.input.prompt = prompt ? sdup(prompt) : NULL;
    return n;
}

ASTNode *ast_if(ASTNode *cond, ASTNode *then, ASTNode *els) {
    ASTNode *n = ast_alloc(NODE_IF);
    n->data.if_stmt.cond = cond;
    n->data.if_stmt.then = then;
    n->data.if_stmt.els = els;
    n->data.if_stmt.flags = 0;
    n->data.if_stmt.has_mode = 0;
    n->data.if_stmt.has_item = NULL;
    n->data.if_stmt.has_items = NULL;
    n->data.if_stmt.has_nitems = 0;
    return n;
}

ASTNode *ast_while(ASTNode *cond, ASTNode *body) {
    ASTNode *n = ast_alloc(NODE_WHILE);
    n->data.while_stmt.cond = cond;
    n->data.while_stmt.body = body;
    return n;
}

ASTNode *ast_forto(const char *var, ASTNode *start, ASTNode *end, ASTNode *body) {
    ASTNode *n = ast_alloc(NODE_FORTO);
    n->data.forto.var = sdup(var);
    n->data.forto.start = start;
    n->data.forto.end = end;
    n->data.forto.body = body;
    return n;
}

ASTNode *ast_loop(ASTNode *body) {
    ASTNode *n = ast_alloc(NODE_LOOP);
    n->data.loop.body = body;
    return n;
}

ASTNode *ast_stop(void) {
    return ast_alloc(NODE_STOP);
}

ASTNode *ast_include(const char *path, char **funcs, int nfuncs) {
    ASTNode *n = ast_alloc(NODE_INCLUDE);
    n->data.include.path = sdup(path);
    n->data.include.funcs = funcs;
    n->data.include.nfuncs = nfuncs;
    return n;
}

ASTNode *ast_function(const char *lib, char **args, int argc) {
    ASTNode *n = ast_alloc(NODE_FUNCTION);
    n->data.funcall.lib = sdup(lib);
    n->data.funcall.args = args;
    n->data.funcall.argc = argc;
    return n;
}

ASTNode *ast_templ(const char *raw) {
    ASTNode *n = ast_alloc(NODE_TEMPLATE);
    n->data.templ.raw = sdup(raw);
    return n;
}

ASTNode *ast_func_def(const char *name, ASTNode *body) {
    ASTNode *n = ast_alloc(NODE_FUNC_DEF);
    n->data.func_def.name = sdup(name);
    n->data.func_def.lib = NULL;
    n->data.func_def.body = body;
    n->data.func_def.param_types = NULL;
    n->data.func_def.return_type = NULL;
    return n;
}

ASTNode *ast_func_call(const char *name) {
    ASTNode *n = ast_alloc(NODE_FUNC_CALL);
    n->data.func_call.name = sdup(name);
    return n;
}

ASTNode *ast_block(void) {
    ASTNode *n = ast_alloc(NODE_BLOCK);
    n->data.block.stmts = safe_alloc(sizeof(ASTNode*) * 16);
    n->data.block.count = 0;
    n->data.block.cap = 16;
    return n;
}

void block_add(ASTNode *block, ASTNode *stmt) {
    if (stmt && stmt->type != NODE_EMPTY) {
        if (block->data.block.count >= block->data.block.cap) {
            block->data.block.cap *= 2;
            block->data.block.stmts = realloc(block->data.block.stmts,
                sizeof(ASTNode*) * block->data.block.cap);
            if (!block->data.block.stmts) fatal("out of memory");
        }
        block->data.block.stmts[block->data.block.count++] = stmt;
    }
}

ASTNode *parse_program(void) {
    ASTNode *prog = ast_block();
    while (lex_cur.type != TOK_EOF) {
        ASTNode *s = parse_stmt();
        if (s->type != NODE_EMPTY) block_add(prog, s);
        while (lex_cur.type == TOK_SEMICOLON) { lex_next(); }
        if (lex_cur.type == TOK_NEWLINE) lex_next();
    }
    return prog;
}

ASTNode *parse_block(void) {
    ASTNode *b = ast_block();
    lex_next();
    while (lex_cur.type != TOK_RBRACE && lex_cur.type != TOK_EOF) {
        if (lex_cur.type == TOK_NEWLINE) { lex_next(); continue; }
        ASTNode *s = parse_stmt();
        if (s->type != NODE_EMPTY) block_add(b, s);
        while (lex_cur.type == TOK_SEMICOLON) { lex_next(); }
        if (lex_cur.type == TOK_NEWLINE) lex_next();
    }
    if (lex_cur.type != TOK_RBRACE) fatal("line %d: expected '}'", lex_cur.line);
    lex_next();
    return b;
}

ASTNode *parse_else_chain(void) {
    if (lex_cur.type == TOK_ELIF) {
        lex_next();
        ASTNode *cond, *body;
        int flags = 0, has_mode = 0;
        ASTNode *has_item = NULL, **has_items = NULL;
        int has_nitems = 0;

        cond = parse_expr();

        if (lex_cur.type == TOK_HAS) {
            has_mode = 1;
            lex_next();
            if (lex_cur.type == TOK_LBRACKET) {
                lex_next();
                int cap = 0;
                while (lex_cur.type != TOK_RBRACKET && lex_cur.type != TOK_EOF) {
                    if (has_nitems >= cap) {
                        cap = cap ? cap * 2 : 4;
                        has_items = realloc(has_items, sizeof(ASTNode*) * cap);
                        if (!has_items) fatal("out of memory");
                    }
                    has_items[has_nitems++] = parse_expr();
                    if (lex_cur.type == TOK_COMMA) lex_next();
                }
                if (lex_cur.type != TOK_RBRACKET) fatal("line %d: expected ']'", lex_cur.line);
                lex_next();
            } else {
                has_item = parse_expr();
            }
            if (lex_cur.type == TOK_NOCASE) { flags |= 1; lex_next(); }
        } else {
            while (lex_cur.type == TOK_NOCASE || lex_cur.type == TOK_ANYWHERE || lex_cur.type == TOK_WORD) {
                if (lex_cur.type == TOK_NOCASE) flags |= 1;
                else if (lex_cur.type == TOK_ANYWHERE) flags |= 2;
                else if (lex_cur.type == TOK_WORD) flags |= 4;
                lex_next();
            }
        }

        if (lex_cur.type == TOK_NEWLINE) lex_next();
        if (lex_cur.type != TOK_LBRACE) fatal("line %d: expected '{' after elif condition", lex_cur.line);
        body = parse_block();
        ASTNode *rest = NULL;
        while (lex_cur.type == TOK_NEWLINE) lex_next();
        if (lex_cur.type == TOK_ELIF || lex_cur.type == TOK_ELSE) {
            rest = parse_else_chain();
        }

        ASTNode *n = ast_if(cond, body, rest);
        n->data.if_stmt.flags = flags;
        n->data.if_stmt.has_mode = has_mode;
        n->data.if_stmt.has_item = has_item;
        n->data.if_stmt.has_items = has_items;
        n->data.if_stmt.has_nitems = has_nitems;
        return n;
    }
    if (lex_cur.type == TOK_ELSE) {
        lex_next();
        while (lex_cur.type == TOK_NEWLINE) lex_next();
        if (lex_cur.type == TOK_IF) {
            return parse_stmt();
        }
        if (lex_cur.type != TOK_LBRACE) fatal("line %d: expected '{' after else", lex_cur.line);
        return parse_block();
    }
    return NULL;
}

ASTNode *parse_stmt(void) {
    if (lex_cur.type == TOK_NEWLINE || lex_cur.type == TOK_EOF) return ast_alloc(NODE_EMPTY);
    if (lex_cur.type == TOK_SEMICOLON) { lex_next(); return ast_alloc(NODE_EMPTY); }

    if (lex_cur.type == TOK_LBRACE) {
        int sp2, sl2, sh2; Token sc2;
        lex_getpos(&sp2, &sl2, &sh2, &sc2);
        lex_next();
        while (lex_cur.type == TOK_ID || lex_cur.type == TOK_STR) {
            lex_next();
            if (lex_cur.type == TOK_COMMA) lex_next();
            else break;
        }
        if (lex_cur.type == TOK_COLON) {
            lex_setpos(sp2, sl2, sh2, sc2);
            return parse_expr();
        }
        if (lex_cur.type == TOK_RBRACE) {
            lex_next();
            if (lex_cur.type == TOK_ASSIGN) {
                lex_setpos(sp2, sl2, sh2, sc2);
                ASTNode *de = parse_expr();
                if (de->type == NODE_DESTRUCT) return de;
                return de;
            }
        }
        lex_setpos(sp2, sl2, sh2, sc2);
        return parse_block();
    }

    if (lex_cur.type == TOK_QMARK) {
        lex_next();
        if (lex_cur.type == TOK_ID && lex_peek_next().type == TOK_ASSIGN) {
            lex_next(); lex_next();
            ASTNode *val = parse_expr();
            ASTNode *n = ast_alloc(NODE_SET_UNDEF);
            n->data.assign.value = val;
            return n;
        }
        fatal("line %d: expected 'name = value' after '?'", lex_cur.line);
        return ast_alloc(NODE_EMPTY);
    }

    if (lex_cur.type == TOK_PRINT) {
        lex_next();
        ASTNode **exprs = NULL;
        int count = 0, cap = 0;
        while (lex_cur.type != TOK_NEWLINE && lex_cur.type != TOK_EOF && lex_cur.type != TOK_RBRACE) {
            if (count > 0) {
                if (lex_cur.type == TOK_COMMA) { lex_next(); }
                else break;
            }
            if (cap <= count) {
                cap = cap ? cap * 2 : 8;
                exprs = realloc(exprs, sizeof(ASTNode*) * cap);
                if (!exprs) fatal("out of memory");
            }
            exprs[count++] = parse_expr();
        }
        if (count == 0) fatal("line %d: print expects an expression", lex_cur.line);
        return ast_print(exprs, count);
    }

    if (lex_cur.type == TOK_ID && (!strcmp(lex_cur.val.str, "int") || !strcmp(lex_cur.val.str, "str"))
        && lex_peek_next().type == TOK_INPUT) {
        int is_int = !strcmp(lex_cur.val.str, "int");
        lex_next(); lex_next();
        char *prompt = NULL;
        if (lex_cur.type == TOK_STR) {
            prompt = sdup(lex_cur.val.str);
            lex_next();
        }
        if (lex_cur.type != TOK_ID) fatal("line %d: int/str input expects a variable name", lex_cur.line);
        ASTNode *n = ast_alloc(NODE_INPUT);
        n->data.input.name = sdup(lex_cur.val.str);
        n->data.input.prompt = prompt;
        n->data.input.force_type = is_int ? FORCE_INPUT_INT : FORCE_INPUT_STR;
        lex_next();
        return n;
    }

    if (lex_cur.type == TOK_INPUT) {
        lex_next();
        char *prompt = NULL;
        if (lex_cur.type == TOK_STR) {
            prompt = sdup(lex_cur.val.str);
            lex_next();
        }
        if (lex_cur.type != TOK_ID) fatal("line %d: input expects a variable name", lex_cur.line);
        char *name = sdup(lex_cur.val.str);
        lex_next();
        ASTNode *n = ast_input(name, prompt);
        if (prompt) free(prompt);
        return n;
    }

    if (lex_cur.type == TOK_IF) {
        lex_next();
        ASTNode *cond, *then, *els = NULL;
        int flags = 0, has_mode = 0;
        ASTNode *has_item = NULL, **has_items = NULL;
        int has_nitems = 0;

        cond = parse_expr();

        if (lex_cur.type == TOK_HAS) {
            has_mode = 1;
            lex_next();
            if (lex_cur.type == TOK_LBRACKET) {
                lex_next();
                int cap = 0;
                while (lex_cur.type != TOK_RBRACKET && lex_cur.type != TOK_EOF) {
                    if (has_nitems >= cap) {
                        cap = cap ? cap * 2 : 4;
                        has_items = realloc(has_items, sizeof(ASTNode*) * cap);
                        if (!has_items) fatal("out of memory");
                    }
                    has_items[has_nitems++] = parse_expr();
                    if (lex_cur.type == TOK_COMMA) lex_next();
                }
                if (lex_cur.type != TOK_RBRACKET) fatal("line %d: expected ']'", lex_cur.line);
                lex_next();
            } else {
                has_item = parse_expr();
            }
            if (lex_cur.type == TOK_NOCASE) { flags |= 1; lex_next(); }
        } else {
            while (lex_cur.type == TOK_NOCASE || lex_cur.type == TOK_ANYWHERE || lex_cur.type == TOK_WORD) {
                if (lex_cur.type == TOK_NOCASE) flags |= 1;
                else if (lex_cur.type == TOK_ANYWHERE) flags |= 2;
                else if (lex_cur.type == TOK_WORD) flags |= 4;
                lex_next();
            }
        }

        if (lex_cur.type == TOK_NEWLINE) lex_next();
        if (lex_cur.type != TOK_LBRACE) fatal("line %d: expected '{' after if condition", lex_cur.line);
        then = parse_block();
        while (lex_cur.type == TOK_NEWLINE) lex_next();
        if (lex_cur.type == TOK_ELIF || lex_cur.type == TOK_ELSE) {
            els = parse_else_chain();
        }

        ASTNode *n = ast_if(cond, then, els);
        n->data.if_stmt.flags = flags;
        n->data.if_stmt.has_mode = has_mode;
        n->data.if_stmt.has_item = has_item;
        n->data.if_stmt.has_items = has_items;
        n->data.if_stmt.has_nitems = has_nitems;
        return n;
    }

    if (lex_cur.type == TOK_WHILE) {
        lex_next();
        ASTNode *cond = parse_expr();
        if (lex_cur.type == TOK_NEWLINE) lex_next();
        if (lex_cur.type != TOK_LBRACE) fatal("line %d: expected '{' after while condition", lex_cur.line);
        ASTNode *body = parse_block();
        return ast_while(cond, body);
    }

    if (lex_cur.type == TOK_FOR) {
        lex_next();
        if (lex_cur.type != TOK_ID) fatal("line %d: for expects a variable name", lex_cur.line);
        char *var = sdup(lex_cur.val.str);
        lex_next();
        if (lex_cur.type != TOK_ASSIGN) fatal("line %d: for expects '=' after variable", lex_cur.line);
        lex_next();
        ASTNode *start = parse_expr();
        if (lex_cur.type != TOK_TO) fatal("line %d: for expects 'to' after start expression", lex_cur.line);
        lex_next();
        ASTNode *end = parse_expr();
        if (lex_cur.type == TOK_NEWLINE) lex_next();
        if (lex_cur.type != TOK_LBRACE) fatal("line %d: expected '{' after for clause", lex_cur.line);
        ASTNode *body = parse_block();
        return ast_forto(var, start, end, body);
    }

    if (lex_cur.type == TOK_EXIT) {
        lex_next();
        return ast_alloc(NODE_EXIT);
    }

    if (lex_cur.type == TOK_LOOP) {
        lex_next();
        if (lex_cur.type == TOK_NEWLINE) lex_next();
        if (lex_cur.type != TOK_LBRACE) fatal("line %d: expected '{' after loop", lex_cur.line);
        ASTNode *body = parse_block();
        return ast_loop(body);
    }

    if (lex_cur.type == TOK_STOP) {
        lex_next();
        return ast_stop();
    }
    if (lex_cur.type == TOK_BREAK) {
        lex_next();
        return ast_alloc(NODE_BREAK);
    }
    if (lex_cur.type == TOK_CONTINUE) {
        lex_next();
        return ast_alloc(NODE_CONTINUE);
    }
    if (lex_cur.type == TOK_TRY) {
        lex_next();
        if (lex_cur.type == TOK_NEWLINE) lex_next();
        if (lex_cur.type != TOK_LBRACE) fatal("line %d: expected '{' after try", lex_cur.line);
        ASTNode *body = parse_block();
        while (lex_cur.type == TOK_NEWLINE) lex_next();
        if (lex_cur.type != TOK_CATCH) fatal("line %d: expected catch after try", lex_cur.line);
        lex_next();
        char *catch_var = NULL;
        if (lex_cur.type == TOK_ID) {
            catch_var = sdup(lex_cur.val.str);
            lex_next();
        }
        if (lex_cur.type == TOK_NEWLINE) lex_next();
        if (lex_cur.type != TOK_LBRACE) fatal("line %d: expected '{' after catch", lex_cur.line);
        ASTNode *catch_body = parse_block();
        ASTNode *n = ast_alloc(NODE_TRY);
        n->data.try_stmt.body = body;
        n->data.try_stmt.catch_body = catch_body;
        n->data.try_stmt.catch_var = catch_var;
        return n;
    }
    if (lex_cur.type == TOK_INCLUDE) {
        lex_next();
        if (lex_cur.type == TOK_STAR) {
            lex_next();
            ASTNode *n = ast_alloc(NODE_INCLUDE);
            n->data.include.path = sdup("*");
            n->data.include.funcs = NULL;
            n->data.include.nfuncs = 0;
            return n;
        }
        if (lex_cur.type == TOK_ID && lib_idx(lex_cur.val.str) >= 0) {
            int li = lib_idx(lex_cur.val.str);
            lib_imported[li] = 1;
            lex_next();
            return ast_alloc(NODE_EMPTY);
        }

        char *path = NULL;
        if (lex_cur.type == TOK_SLASH) {
            path = malloc(2);
            if (!path) fatal("out of memory");
            strcpy(path, "/");
            lex_next();
            while (lex_cur.type == TOK_ID || lex_cur.type == TOK_SLASH) {
                size_t olen = strlen(path);
                size_t tlen = lex_cur.type == TOK_SLASH ? 1 : strlen(lex_cur.val.str);
                path = realloc(path, olen + tlen + 2);
                if (!path) fatal("out of memory");
                if (lex_cur.type == TOK_SLASH) strcat(path, "/");
                else strcat(path, lex_cur.val.str);
                lex_next();
            }
            if (lex_cur.type == TOK_DOT) {
                lex_next();
                if (lex_cur.type != TOK_ID) fatal("line %d: expected extension after '.'", lex_cur.line);
                size_t olen = strlen(path);
                size_t tlen = lex_cur.type == TOK_SLASH ? 1 : strlen(lex_cur.val.str);
                path = realloc(path, olen + tlen + 2);
                if (!path) fatal("out of memory");
                strcat(path, ".");
                strcat(path, lex_cur.val.str);
                lex_next();
            }
        } else if (lex_cur.type == TOK_ID) {
            size_t plen = strlen(lex_cur.val.str);
            path = malloc(plen + 1);
            if (!path) fatal("out of memory");
            strcpy(path, lex_cur.val.str);
            lex_next();
            if (lex_cur.type == TOK_DOT) {
                lex_next();
                if (lex_cur.type != TOK_ID) fatal("line %d: expected extension after '.'", lex_cur.line);
                plen += 1 + strlen(lex_cur.val.str);
                path = realloc(path, plen + 1);
                if (!path) fatal("out of memory");
                strcat(path, ".");
                strcat(path, lex_cur.val.str);
                lex_next();
            }
        } else {
            fatal("line %d: expected file path after include", lex_cur.line);
            return ast_alloc(NODE_EMPTY);
        }

        char **funcs = NULL;
        int nfuncs = 0, fcap = 0;
        if (lex_cur.type == TOK_SLASH) {
            lex_next();
            while (lex_cur.type == TOK_ID) {
                if (nfuncs >= fcap) {
                    fcap = fcap ? fcap * 2 : 4;
                    funcs = realloc(funcs, sizeof(char*) * fcap);
                    if (!funcs) fatal("out of memory");
                }
                funcs[nfuncs++] = sdup(lex_cur.val.str);
                lex_next();
            }
            if (nfuncs == 0) fatal("line %d: expected function names after '/'", lex_cur.line);
        }

        ASTNode *n = ast_alloc(NODE_INCLUDE);
        n->data.include.path = path;
        n->data.include.funcs = funcs;
        n->data.include.nfuncs = nfuncs;
        return n;
    }

    if (lex_cur.type == TOK_UNINCLUDE) {
        lex_next();
        if (lex_cur.type == TOK_ID && lib_idx(lex_cur.val.str) >= 0) {
            int li = lib_idx(lex_cur.val.str);
            if (!lib_imported[li])
                /* library not included warning stripped */
            lib_imported[li] = 0;
            lex_next();
            return ast_alloc(NODE_EMPTY);
        }
        fatal("line %d: expected library name after drop", lex_cur.line);
        return ast_alloc(NODE_EMPTY);
    }
    if (lex_cur.type == TOK_STRUCT) {
        lex_next();
        if (lex_cur.type != TOK_ID) fatal("line %d: expected struct name", lex_cur.line);
        ASTNode *n = ast_alloc(NODE_STRUCT_DEF);
        n->data.struct_def.name = sdup(lex_cur.val.str);
        lex_next();
        if (lex_cur.type == TOK_NEWLINE) lex_next();
        if (lex_cur.type != TOK_LBRACE) fatal("line %d: expected '{' after struct name", lex_cur.line);
        lex_next();
        char **fields = NULL;
        int nf = 0, fcap = 0;
        while (lex_cur.type != TOK_RBRACE && lex_cur.type != TOK_EOF) {
            if (lex_cur.type == TOK_NEWLINE) { lex_next(); continue; }
            if (lex_cur.type != TOK_ID) fatal("line %d: expected field name", lex_cur.line);
            if (nf >= fcap) { fcap = fcap ? fcap * 2 : 4; fields = realloc(fields, sizeof(char*) * fcap); if (!fields) fatal("out of memory"); }
            fields[nf++] = sdup(lex_cur.val.str);
            lex_next();
            if (lex_cur.type == TOK_COMMA) lex_next();
        }
        if (lex_cur.type != TOK_RBRACE) fatal("line %d: expected '}'", lex_cur.line);
        lex_next();
        n->data.struct_def.fields = fields;
        n->data.struct_def.nfields = nf;
        return n;
    }
    if (lex_cur.type == TOK_GET) {
        lex_next();
        char **varnames = NULL, **newnames = NULL;
        int *indices = NULL;
        int nvars = 0, vcap = 0;

        while (lex_cur.type == TOK_STR) {
            if (nvars >= vcap) {
                vcap = vcap ? vcap * 2 : 4;
                varnames = realloc(varnames, sizeof(char*) * vcap);
                newnames = realloc(newnames, sizeof(char*) * vcap);
                indices = realloc(indices, sizeof(int) * vcap);
                if (!varnames || !newnames || !indices) fatal("out of memory");
            }
            varnames[nvars] = sdup(lex_cur.val.str);
            newnames[nvars] = NULL;
            indices[nvars] = 0;
            lex_next();

            if (lex_cur.type == TOK_LPAREN) {
                lex_next();
                if (lex_cur.type != TOK_NUM) fatal("line %d: expected number after '('", lex_cur.line);
                indices[nvars] = (int)lex_cur.val.num;
                lex_next();
                if (lex_cur.type != TOK_RPAREN) fatal("line %d: expected ')'", lex_cur.line);
                lex_next();
            }

            if (lex_cur.type == TOK_ASSIGN) {
                lex_next();
                if (lex_cur.type != TOK_STR) fatal("line %d: expected string after '='", lex_cur.line);
                newnames[nvars] = sdup(lex_cur.val.str);
                lex_next();
            }

            nvars++;

            if (lex_cur.type == TOK_COMMA) {
                lex_next();
                continue;
            }
            break;
        }

        if (nvars == 0) fatal("line %d: expected variable name(s) after 'get'", lex_cur.line);

        if (lex_cur.type != TOK_FROM) fatal("line %d: expected 'from' after variable name(s)", lex_cur.line);
        lex_next();

        char *path = NULL;
        if (lex_cur.type == TOK_SLASH) {
            path = malloc(2);
            if (!path) fatal("out of memory");
            strcpy(path, "/");
            lex_next();
            while (lex_cur.type == TOK_ID || lex_cur.type == TOK_SLASH) {
                size_t olen = strlen(path);
                size_t tlen = lex_cur.type == TOK_SLASH ? 1 : strlen(lex_cur.val.str);
                path = realloc(path, olen + tlen + 2);
                if (!path) fatal("out of memory");
                if (lex_cur.type == TOK_SLASH) strcat(path, "/");
                else strcat(path, lex_cur.val.str);
                lex_next();
            }
            if (lex_cur.type == TOK_DOT) {
                lex_next();
                if (lex_cur.type != TOK_ID) fatal("line %d: expected file extension after '.'", lex_cur.line);
                size_t olen = strlen(path);
                size_t tlen = lex_cur.type == TOK_SLASH ? 1 : strlen(lex_cur.val.str);
                path = realloc(path, olen + tlen + 2);
                if (!path) fatal("out of memory");
                strcat(path, ".");
                strcat(path, lex_cur.val.str);
                lex_next();
            }
        } else {
            if (lex_cur.type != TOK_ID) fatal("line %d: expected file path after 'from'", lex_cur.line);
            size_t plen = strlen(lex_cur.val.str);
            path = malloc(plen + 1);
            if (!path) fatal("out of memory");
            strcpy(path, lex_cur.val.str);
            lex_next();

            if (lex_cur.type == TOK_DOT) {
                lex_next();
                if (lex_cur.type != TOK_ID) fatal("line %d: expected file extension after '.'", lex_cur.line);
                plen += 1 + strlen(lex_cur.val.str);
                path = realloc(path, plen + 1);
                if (!path) fatal("out of memory");
                strcat(path, ".");
                strcat(path, lex_cur.val.str);
                lex_next();
            }
        }

        ASTNode *n = ast_alloc(NODE_GET);
        n->data.get_stmt.varnames = varnames;
        n->data.get_stmt.newnames = newnames;
        n->data.get_stmt.nvars = nvars;
        n->data.get_stmt.path = path;
        n->data.get_stmt.indices = indices;
        return n;
    }
    if (lex_cur.type == TOK_STRINGIFY) {
        int sline = lex_cur.line;
        lex_next();
        if (lex_cur.type != TOK_ID) fatal("line %d: stringify expects a variable name", lex_cur.line);
        char *name = sdup(lex_cur.val.str);
        lex_next();
        ASTNode *n = ast_alloc(NODE_STRINGIFY);
        n->data.modify.name = name;
        n->data.modify.fmt = NULL;
        n->data.modify.value = NULL;
        if (lex_cur.type == TOK_ASSIGN) {
            static int stringify_warned;
                /* stripped */
            lex_next();
            n->data.modify.value = parse_expr();
        }
        return n;
    }
    if (lex_cur.type == TOK_INTIFY) {
        int sline = lex_cur.line;
        lex_next();
        if (lex_cur.type != TOK_ID) fatal("line %d: intify expects a variable name", lex_cur.line);
        char *name = sdup(lex_cur.val.str);
        lex_next();
        char *fmt = NULL;
        if (lex_cur.type == TOK_ID && (!strcmp(lex_cur.val.str, "binary") || !strcmp(lex_cur.val.str, "hex") || !strcmp(lex_cur.val.str, "octal"))) {
            fmt = sdup(lex_cur.val.str);
            lex_next();
        }
        ASTNode *n = ast_alloc(NODE_INTIFY);
        n->data.modify.name = name;
        n->data.modify.fmt = fmt;
        n->data.modify.value = NULL;
        if (lex_cur.type == TOK_ASSIGN) {
            static int intify_warned;
                /* stripped */
            lex_next();
            n->data.modify.value = parse_expr();
        }
        return n;
    }

    if (lex_cur.type == TOK_TOGGLE) {
        int sline = lex_cur.line;
        lex_next();
        if (lex_cur.type != TOK_ID) fatal("line %d: toggle expects a variable name", lex_cur.line);
        char *name = sdup(lex_cur.val.str);
        lex_next();
        ASTNode *n = ast_alloc(NODE_TOGGLE);
        n->data.modify.name = name;
        n->data.modify.fmt = NULL;
        n->data.modify.value = NULL;
        if (lex_cur.type == TOK_ASSIGN) {
            static int toggle_warned;
                /* stripped */
            lex_next();
            n->data.modify.value = parse_expr();
        }
        return n;
    }

    if (lex_cur.type == TOK_TYPED) {
        lex_next();
        char *typename = NULL;
        if (lex_cur.type == TOK_ID && (!strcmp(lex_cur.val.str, "int") || !strcmp(lex_cur.val.str, "str"))) {
            typename = sdup(lex_cur.val.str);
            lex_next();
        } else if (lex_cur.type == TOK_ID) {
            fatal("line %d: unsupported type '%s', use 'int' or 'str'", lex_cur.line, lex_cur.val.str);
        }
        if (lex_cur.type != TOK_ID) fatal("line %d: typed expects a variable name", lex_cur.line);
        char *name = sdup(lex_cur.val.str);
        lex_next();
        ASTNode *val = NULL;
        if (lex_cur.type == TOK_ASSIGN) {
            lex_next();
            val = parse_expr();
        }
        ASTNode *n = ast_alloc(NODE_ASSIGN);
        n->data.assign.name = name;
        n->data.assign.value = val ? val : ast_num(0);
        n->data.assign.typed = typename;
        return n;
    }

    if (lex_cur.type == TOK_MOD && lex_peek_next().type == TOK_ID) {
        lex_next();
        char *vname = sdup(lex_cur.val.str);
        lex_next();
        return ast_assign(vname, ast_num(0));
    }
    if (lex_cur.type == TOK_LIVE) {
        lex_next();
        if (lex_cur.type != TOK_ID) fatal("line %d: live expects a variable name", lex_cur.line);
        char *name = sdup(lex_cur.val.str);
        lex_next();
        if (lex_cur.type != TOK_ASSIGN) fatal("line %d: expected '=' after live variable", lex_cur.line);
        lex_next();
        ASTNode *val = parse_expr();
        ASTNode *n = ast_alloc(NODE_LIVE);
        n->data.assign.name = name;
        n->data.assign.value = val;
        return n;
    }

    if (lex_cur.type == TOK_ID && lex_peek_next().type == TOK_ASSIGN) {
        char *name = sdup(lex_cur.val.str);
        lex_next();
        lex_next();
        ASTNode *val = parse_expr();
        return ast_assign(name, val);
    }

    if (lex_cur.type == TOK_FORCE) {
        lex_next();
        if (lex_cur.type == TOK_ID && (!strcmp(lex_cur.val.str, "int") || !strcmp(lex_cur.val.str, "str"))
            && lex_peek_next().type == TOK_INPUT) {
            int is_int = !strcmp(lex_cur.val.str, "int");
            lex_next(); lex_next();
            char *prompt = NULL;
            if (lex_cur.type == TOK_STR) {
                prompt = sdup(lex_cur.val.str);
                lex_next();
            }
            if (lex_cur.type != TOK_ID) fatal("line %d: force input expects a variable name", lex_cur.line);
            ASTNode *n = ast_alloc(NODE_INPUT);
            n->data.input.name = sdup(lex_cur.val.str);
            n->data.input.prompt = prompt;
            n->data.input.force_type = is_int ? FORCE_INT : FORCE_STR;
            lex_next();
            return n;
        }
        if (lex_cur.type == TOK_ID && (!strcmp(lex_cur.val.str, "int") || !strcmp(lex_cur.val.str, "str"))
            && lex_peek_next().type == TOK_ID) {
            int is_int = !strcmp(lex_cur.val.str, "int");
            lex_next();
            ASTNode *n = ast_alloc(NODE_FORCE);
            n->data.force.name = sdup(lex_cur.val.str);
            n->data.force.force_type = is_int ? FORCE_INT : FORCE_STR;
            n->data.force.value = NULL;
            lex_next();
            if (lex_cur.type == TOK_ASSIGN) {
                lex_next();
                n->data.force.value = parse_expr();
            }
            return n;
        }
        if (lex_cur.type != TOK_ID) fatal("line %d: force expects a variable name", lex_cur.line);
        ASTNode *n = ast_alloc(NODE_FORCE);
        n->data.force.name = sdup(lex_cur.val.str);
        n->data.force.force_type = FORCE_NONE;
        lex_next();
        n->data.force.value = NULL;
        if (lex_cur.type == TOK_ASSIGN) {
            lex_next();
            n->data.force.value = parse_expr();
        }
        return n;
    }
    if (lex_cur.type == TOK_UNFORCE) {
        lex_next();
        if (lex_cur.type != TOK_ID) fatal("line %d: unforce expects a variable name", lex_cur.line);
        ASTNode *n = ast_alloc(NODE_UNFORCE);
        n->data.force.name = sdup(lex_cur.val.str);
        n->data.force.force_type = 0;
        lex_next();
        n->data.force.value = NULL;
        if (lex_cur.type == TOK_ASSIGN) {
            lex_next();
            n->data.force.value = parse_expr();
        }
        return n;
    }

    ASTNode *e = parse_expr();
    if (e->type == NODE_FUNC_CALL) {
        while (lex_cur.type == TOK_NEWLINE) lex_next();
        if (lex_cur.type == TOK_LBRACE) {
            if (e->data.func_call.name && (!strcmp(e->data.func_call.name, "newCreativeTab")
                || !strcmp(e->data.func_call.name, "recipeShaped")
                || !strcmp(e->data.func_call.name, "recipeShapeless")
                || !strcmp(e->data.func_call.name, "recipeSmelting"))) {
                int depth = 1; lex_next();
                while (lex_cur.type != TOK_EOF && depth > 0) {
                    if (lex_cur.type == TOK_LBRACE) depth++;
                    if (lex_cur.type == TOK_RBRACE) { depth--; if (!depth) break; }
                    lex_next();
                }
                if (lex_cur.type == TOK_RBRACE) lex_next();
                return ast_alloc(NODE_EMPTY);
            }
            int np = e->data.func_call.nargs;
            char **params = malloc(np * sizeof(char*));
            char **ptypes = e->data.func_call.param_types;
            for (int i = 0; i < np; i++) {
                if (e->data.func_call.args[i]->type != NODE_ID)
                    fatal("line %d: parameter must be an identifier", e->data.func_call.args[i]->line);
                params[i] = sdup(e->data.func_call.args[i]->data.id);
            }
            ASTNode *body = parse_block();
            ASTNode *n = ast_alloc(NODE_FUNC_DEF);
            n->data.func_def.name = e->data.func_call.name;
            e->data.func_call.name = NULL;
            n->data.func_def.params = params;
            n->data.func_def.nparams = np;
            n->data.func_def.body = body;
            n->data.func_def.lib = e->data.func_call.lib;
            e->data.func_call.lib = NULL;
            n->data.func_def.param_types = ptypes;
            e->data.func_call.param_types = NULL;
            n->data.func_def.return_type = e->data.func_call.return_type;
            e->data.func_call.return_type = NULL;
            return n;
        }
    }

    if (e->type == NODE_FUNCTION && e->data.funcall.lib) {
        while (lex_cur.type == TOK_NEWLINE) lex_next();
        if (lex_cur.type == TOK_LBRACE) {
            char *fname = e->data.funcall.args[0];
            if (fname && !strcmp(fname, "itemNames")) {
                ASTNode *n = ast_alloc(NODE_FUNC_DEF);
                n->data.func_def.name = sdup(fname);
                n->data.func_def.lib = sdup(e->data.funcall.lib);
                n->data.func_def.body = NULL;
                n->data.func_def.params = NULL;
                n->data.func_def.nparams = 0;
                int depth = 1; lex_next();
                while (lex_cur.type != TOK_EOF && depth > 0) {
                    if (lex_cur.type == TOK_LBRACE) depth++;
                    if (lex_cur.type == TOK_RBRACE) { depth--; if (!depth) break; }
                    lex_next();
                }
                if (lex_cur.type == TOK_RBRACE) lex_next();
                return n;
            }
            if (fname && (!strcmp(fname, "newCreativeTab"))) {
                int depth = 1; lex_next();
                while (lex_cur.type != TOK_EOF && depth > 0) {
                    if (lex_cur.type == TOK_LBRACE) depth++;
                    if (lex_cur.type == TOK_RBRACE) { depth--; if (!depth) break; }
                    lex_next();
                }
                if (lex_cur.type == TOK_RBRACE) lex_next();
                return ast_alloc(NODE_EMPTY);
            }
            ASTNode *body = parse_block();
            ASTNode *n = ast_alloc(NODE_FUNC_DEF);
            n->data.func_def.name = sdup(fname);
            n->data.func_def.params = NULL;
            n->data.func_def.nparams = 0;
            n->data.func_def.body = body;
            n->data.func_def.lib = sdup(e->data.funcall.lib);
            return n;
        }
    }

    if (lex_cur.type == TOK_ASSIGN && e->type == NODE_INDEX) {
        lex_next();
        ASTNode *n = ast_alloc(NODE_INDEX_SET);
        n->data.idx_set.container = e->data.idx.container;
        n->data.idx_set.index = e->data.idx.index;
        n->data.idx_set.value = parse_expr();
        return n;
    }

    if (lex_cur.type == TOK_ASSIGN && e->type == NODE_FUNCTION) {
        if (e->data.funcall.lib && !strcmp(e->data.funcall.lib, "minecraft")) {
            lex_next();
            while (lex_cur.type != TOK_NEWLINE && lex_cur.type != TOK_EOF && lex_cur.type != TOK_RBRACE)
                lex_next();
            return ast_alloc(NODE_EMPTY);
        }
        fatal("line %d: cannot assign to function expression", e->line);
    }

    return e;
}

ASTNode *parse_primary(void) {
    if (lex_cur.type == TOK_LBRACKET) {
        lex_next();
        ASTNode **elements = NULL;
        int count = 0, cap = 0;
        while (lex_cur.type != TOK_RBRACKET && lex_cur.type != TOK_EOF) {
            if (count >= cap) { cap = cap ? cap * 2 : 4; elements = realloc(elements, sizeof(ASTNode*) * cap); if (!elements) fatal("out of memory"); }
            elements[count++] = parse_expr();
            if (lex_cur.type == TOK_COMMA) lex_next();
        }
        if (lex_cur.type != TOK_RBRACKET) fatal("line %d: expected ']'", lex_cur.line);
        lex_next();
        ASTNode *n = ast_alloc(NODE_LIST);
        n->data.list.elements = elements;
        n->data.list.count = count;
        return n;
    }
    if (lex_cur.type == TOK_LBRACE) {
        char *names[64];
        int nn = 0;
        int sp, sl, sh; Token sc;
        lex_getpos(&sp, &sl, &sh, &sc);
        lex_next();
        while (lex_cur.type == TOK_ID || lex_cur.type == TOK_STR) {
            if (nn >= 64) break;
            names[nn++] = sdup(lex_cur.val.str);
            lex_next();
            if (lex_cur.type == TOK_COMMA) lex_next();
            else break;
        }
        if (lex_cur.type == TOK_RBRACE) {
            lex_next();
            if (lex_cur.type == TOK_ASSIGN && nn > 0) {
                char **fields = malloc(nn * sizeof(char*));
                for (int i = 0; i < nn; i++) fields[i] = names[i];
                lex_next();
                ASTNode *n = ast_alloc(NODE_DESTRUCT);
                n->data.destruct.fields = fields;
                n->data.destruct.nfields = nn;
                n->data.destruct.source = parse_expr();
                return n;
            }
        }
        for (int i = 0; i < nn; i++) free(names[i]);
        lex_setpos(sp, sl, sh, sc);
        lex_next();
        ASTNode **keys = NULL;
        ASTNode **values = NULL;
        int count = 0, cap = 0;
        while (lex_cur.type != TOK_RBRACE && lex_cur.type != TOK_EOF) {
            if (count >= cap) { cap = cap ? cap * 2 : 4; keys = realloc(keys, sizeof(ASTNode*) * cap); values = realloc(values, sizeof(ASTNode*) * cap); if (!keys || !values) fatal("out of memory"); }
            keys[count] = parse_expr();
            if (lex_cur.type != TOK_COLON) fatal("line %d: expected ':' after dict key", lex_cur.line);
            lex_next();
            values[count] = parse_expr();
            count++;
            if (lex_cur.type == TOK_COMMA) lex_next();
        }
        if (lex_cur.type != TOK_RBRACE) fatal("line %d: expected '}'", lex_cur.line);
        lex_next();
        ASTNode *n = ast_alloc(NODE_DICT);
        n->data.dict.keys = keys;
        n->data.dict.values = values;
        n->data.dict.count = count;
        return n;
    }
    if (lex_cur.type == TOK_NUM) {
        double v = lex_cur.val.num;
        lex_next();
        return ast_num(v);
    }
    if (lex_cur.type == TOK_STR) {
        char *s = sdup(lex_cur.val.str);
        lex_next();
        return ast_str(s);
    }
    if (lex_cur.type == TOK_TEMPLATE) {
        char *r = sdup(lex_cur.val.str);
        lex_next();
        return ast_templ(r);
    }
    if (lex_cur.type == TOK_AMPERSAND) {
        static int amp_warned;
        /* deprecated syntax warning stripped */
        lex_next();
        if (lex_cur.type != TOK_ID) fatal("line %d: expected library name after '&'", lex_cur.line);
        char *lib = sdup(lex_cur.val.str);
        lex_next();
        if (lex_cur.type != TOK_PIPE) fatal("line %d: expected '|' after library name", lex_cur.line);
        lex_next();
        if (lex_cur.type != TOK_ID) fatal("line %d: expected function name after '|'", lex_cur.line);
        char **args = NULL;
        int argc = 0, cap = 0;
        if (argc >= cap) { cap = 4; args = malloc(sizeof(char*) * cap); if (!args) fatal("out of memory"); }
        args[argc++] = sdup(lex_cur.val.str);
        lex_next();
        while (lex_cur.type == TOK_ID || lex_cur.type == TOK_STR || lex_cur.type == TOK_NUM || lex_cur.type == TOK_TEMPLATE || lex_cur.type == TOK_MINUS) {
            if (lex_cur.type == TOK_MINUS) {
                if (lex_peek_next().type != TOK_NUM) break;
                lex_next();
                if (argc >= cap) { cap *= 2; args = realloc(args, sizeof(char*) * cap); if (!args) fatal("out of memory"); }
                char tmp[70]; snprintf(tmp, sizeof(tmp), "-%.10g", lex_cur.val.num);
                args[argc++] = sdup(tmp);
                lex_next();
                continue;
            }
            if (argc >= cap) { cap *= 2; args = realloc(args, sizeof(char*) * cap); if (!args) fatal("out of memory"); }
            if (lex_cur.type == TOK_NUM) {
                char tmp[64]; snprintf(tmp, sizeof(tmp), "%.10g", lex_cur.val.num);
                args[argc++] = sdup(tmp);
            } else if (lex_cur.type == TOK_ID) {
                char *prefixed = malloc(strlen(lex_cur.val.str) + 2);
                if (!prefixed) fatal("out of memory");
                prefixed[0] = '\1';
                strcpy(prefixed + 1, lex_cur.val.str);
                args[argc++] = prefixed;
            } else {
                args[argc++] = sdup(lex_cur.val.str);
            }
            lex_next();
        }
        return ast_function(lib, args, argc);
    }
    if (lex_cur.type == TOK_HASH) {
        lex_next();
        if (lex_cur.type != TOK_LPAREN) fatal("line %d: expected '(' after '#'", lex_cur.line);
        lex_next();
        char **pnames = NULL;
        int npnames = 0, acap = 0;
        while (lex_cur.type != TOK_RPAREN && lex_cur.type != TOK_EOF) {
            if (npnames >= acap) { acap = acap ? acap * 2 : 4; pnames = realloc(pnames, acap * sizeof(char*)); }
            if (lex_cur.type != TOK_ID) fatal("line %d: expected parameter name", lex_cur.line);
            pnames[npnames++] = sdup(lex_cur.val.str);
            lex_next();
            if (lex_cur.type == TOK_COMMA) lex_next();
        }
        if (lex_cur.type != TOK_RPAREN) fatal("line %d: expected ')'", lex_cur.line);
        lex_next();
        while (lex_cur.type == TOK_NEWLINE) lex_next();
        if (lex_cur.type != TOK_LBRACE) fatal("line %d: expected '{' for function body", lex_cur.line);
        ASTNode *body = parse_block();
        ASTNode *n = ast_alloc(NODE_ANON_FUNC);
        n->data.func_def.params = pnames;
        n->data.func_def.nparams = npnames;
        n->data.func_def.body = body;
        n->data.func_def.name = NULL;
        return n;
    }
    if (lex_cur.type == TOK_HASH_ID) {
        char *fname = sdup(lex_cur.val.str);
        lex_next();
        if (lex_cur.type != TOK_LPAREN) fatal("line %d: expected '(' after function name", lex_cur.line);
        lex_next();
        ASTNode **pargs = NULL;
        char **ptypes = NULL;
        int npargs = 0, acap = 0;
        while (lex_cur.type != TOK_RPAREN && lex_cur.type != TOK_EOF) {
            if (npargs >= acap) { acap = acap ? acap * 2 : 4; pargs = realloc(pargs, acap * sizeof(ASTNode*)); ptypes = realloc(ptypes, acap * sizeof(char*)); }
            if (lex_cur.type != TOK_ID) fatal("line %d: expected parameter name", lex_cur.line);
            pargs[npargs] = ast_id(lex_cur.val.str);
            ptypes[npargs] = NULL;
            lex_next();
    if (lex_cur.type == TOK_TYPED) {
                lex_next();
                if (lex_cur.type == TOK_ID && (!strcmp(lex_cur.val.str, "int") || !strcmp(lex_cur.val.str, "str"))) {
                    ptypes[npargs] = sdup(lex_cur.val.str);
                    lex_next();
                }
            }
            npargs++;
            if (lex_cur.type == TOK_COMMA) lex_next();
        }
        if (lex_cur.type != TOK_RPAREN) fatal("line %d: expected ')'", lex_cur.line);
        lex_next();
        char *rtype = NULL;
        if (lex_cur.type == TOK_TYPED) {
            lex_next();
            if (lex_cur.type == TOK_ID && (!strcmp(lex_cur.val.str, "int") || !strcmp(lex_cur.val.str, "str"))) {
                rtype = sdup(lex_cur.val.str);
                lex_next();
            }
        }
        ASTNode *n = ast_alloc(NODE_FUNC_CALL);
        n->data.func_call.name = fname;
        n->data.func_call.args = pargs;
        n->data.func_call.nargs = npargs;
        n->data.func_call.lib = NULL;
        n->data.func_call.param_types = ptypes;
        n->data.func_call.return_type = rtype;
        return n;
    }
    if (lex_cur.type == TOK_ID) {
        char *fname = sdup(lex_cur.val.str);
        if (lex_peek_next().type == TOK_LPAREN) {
            lex_next();
            lex_next();
            ASTNode **args = NULL;
            int nargs = 0, acap = 0;
            while (lex_cur.type != TOK_RPAREN && lex_cur.type != TOK_EOF) {
                if (nargs >= acap) { acap = acap ? acap * 2 : 4; args = realloc(args, acap * sizeof(ASTNode*)); if (!args) fatal("out of memory"); }
                args[nargs++] = parse_expr();
                if (lex_cur.type == TOK_COMMA) lex_next();
            }
            if (lex_cur.type != TOK_RPAREN) fatal("line %d: expected ')'", lex_cur.line);
            lex_next();
            char *lib = NULL;
            if (lex_cur.type == TOK_AT) {
                lex_next();
                if (lex_cur.type != TOK_ID) fatal("line %d: expected library name after '@'", lex_cur.line);
                lib = sdup(lex_cur.val.str);
                lex_next();
            }
            ASTNode *n = ast_alloc(NODE_FUNC_CALL);
            n->data.func_call.name = fname;
            n->data.func_call.args = args;
            n->data.func_call.nargs = nargs;
            n->data.func_call.lib = lib;
            return n;
        }
        if (lex_peek_next().type == TOK_AT) {
            lex_next();
            lex_next();
            if (lex_cur.type != TOK_ID) fatal("line %d: expected library name after '@'", lex_cur.line);
            char *lib = sdup(lex_cur.val.str);
            lex_next();
            if (lex_cur.type == TOK_LPAREN) {
                lex_next();
                ASTNode **args = NULL;
                int nargs = 0, acap = 0;
                while (lex_cur.type != TOK_RPAREN && lex_cur.type != TOK_EOF) {
                    if (nargs >= acap) { acap = acap ? acap * 2 : 4; args = realloc(args, acap * sizeof(ASTNode*)); if (!args) fatal("out of memory"); }
                    args[nargs++] = parse_expr();
                    if (lex_cur.type == TOK_COMMA) lex_next();
                }
                if (lex_cur.type != TOK_RPAREN) fatal("line %d: expected ')'", lex_cur.line);
                lex_next();
                ASTNode *n = ast_alloc(NODE_FUNC_CALL);
                n->data.func_call.name = fname;
                n->data.func_call.args = args;
                n->data.func_call.nargs = nargs;
                n->data.func_call.lib = lib;
                return n;
            }
            char **sargs = NULL;
            int sargc = 0, scap = 0;
            if (scap == 0) { scap = 4; sargs = malloc(sizeof(char*) * scap); if (!sargs) fatal("out of memory"); }
            sargs[sargc++] = fname;
            while (lex_cur.type == TOK_ID || lex_cur.type == TOK_STR || lex_cur.type == TOK_NUM || lex_cur.type == TOK_TEMPLATE || lex_cur.type == TOK_MINUS) {
                if (lex_cur.type == TOK_MINUS) {
                    if (lex_peek_next().type != TOK_NUM) break;
                    lex_next();
                    if (sargc >= scap) { scap *= 2; sargs = realloc(sargs, sizeof(char*) * scap); if (!sargs) fatal("out of memory"); }
                    char tmp[70]; snprintf(tmp, sizeof(tmp), "-%.10g", lex_cur.val.num);
                    sargs[sargc++] = sdup(tmp);
                    lex_next();
                    continue;
                }
                if (sargc >= scap) { scap *= 2; sargs = realloc(sargs, sizeof(char*) * scap); if (!sargs) fatal("out of memory"); }
                if (lex_cur.type == TOK_NUM) {
                    char tmp[64]; snprintf(tmp, sizeof(tmp), "%.10g", lex_cur.val.num);
                    sargs[sargc++] = sdup(tmp);
                } else if (lex_cur.type == TOK_ID) {
                    char *prefixed = malloc(strlen(lex_cur.val.str) + 2);
                    if (!prefixed) fatal("out of memory");
                    prefixed[0] = '\1';
                    strcpy(prefixed + 1, lex_cur.val.str);
                    sargs[sargc++] = prefixed;
                } else {
                    sargs[sargc++] = sdup(lex_cur.val.str);
                }
                lex_next();
            }
            return ast_function(lib, sargs, sargc);
        }
        lex_next();
        return ast_id(fname);
    }
    if (lex_cur.type == TOK_WRITE || lex_cur.type == TOK_READ) {
        char *fname = lex_cur.type == TOK_WRITE ? sdup("write") : sdup("read");
        lex_next();
        ASTNode **wargs = NULL;
        int wargs_n = 0, wargs_cap = 0;
        while (lex_cur.type != TOK_NEWLINE && lex_cur.type != TOK_EOF && lex_cur.type != TOK_RBRACE) {
            if (wargs_n >= wargs_cap) { wargs_cap = wargs_cap ? wargs_cap * 2 : 4; wargs = realloc(wargs, wargs_cap * sizeof(ASTNode*)); if (!wargs) fatal("out of memory"); }
            wargs[wargs_n++] = parse_expr();
            if (lex_cur.type == TOK_COMMA) lex_next();
            else break;
        }
        ASTNode *n = ast_alloc(NODE_FUNC_CALL);
        n->data.func_call.name = fname;
        n->data.func_call.args = wargs;
        n->data.func_call.nargs = wargs_n;
        n->data.func_call.lib = NULL;
        return n;
    }
    if (lex_cur.type == TOK_LPAREN) {
        lex_next();
    ASTNode *e = parse_expr();
        if (lex_cur.type != TOK_RPAREN) fatal("line %d: expected ')'", lex_cur.line);
        lex_next();
        return e;
    }
    if (lex_cur.type == TOK_MINUS) {
        lex_next();
        ASTNode *o = parse_primary();
        return ast_unary(TOK_MINUS, o);
    }
    if (lex_cur.type == TOK_NOT) {
        lex_next();
        ASTNode *o = parse_primary();
        return ast_unary(TOK_NOT, o);
    }
    fatal("line %d: unexpected token", lex_cur.line);
    return NULL;
}

ASTNode *parse_unary(void) {
    if (lex_cur.type == TOK_MINUS) {
        lex_next();
        ASTNode *o = parse_unary();
        return ast_unary(TOK_MINUS, o);
    }
    if (lex_cur.type == TOK_NOT) {
        lex_next();
        ASTNode *o = parse_unary();
        return ast_unary(TOK_NOT, o);
    }
    if (lex_cur.type == TOK_AT) {
        lex_next();
        ASTNode *o = parse_unary();
        return ast_unary(TOK_AT, o);
    }
    if (lex_cur.type == TOK_CARET) {
        lex_next();
        ASTNode *o = parse_unary();
        return ast_unary(TOK_CARET, o);
    }
    return parse_postfix();
}

ASTNode *parse_postfix(void) {
    ASTNode *e = parse_primary();
    while (lex_cur.type == TOK_LBRACKET || lex_cur.type == TOK_DOT) {
        if (lex_cur.type == TOK_DOT) {
            lex_next();
            if (lex_cur.type != TOK_ID) fatal("line %d: expected field or method name after '.'", lex_cur.line);
            if (lex_peek_next().type == TOK_LPAREN) {
                char *method = sdup(lex_cur.val.str);
                lex_next();
                lex_next();
                ASTNode **args = NULL;
                int nargs = 0, acap = 0;
                while (lex_cur.type != TOK_RPAREN && lex_cur.type != TOK_EOF) {
                    if (nargs >= acap) { acap = acap ? acap * 2 : 4; args = realloc(args, acap * sizeof(ASTNode*)); if (!args) fatal("out of memory"); }
                    args[nargs++] = parse_expr();
                    if (lex_cur.type == TOK_COMMA) lex_next();
                }
                if (lex_cur.type != TOK_RPAREN) fatal("line %d: expected ')'", lex_cur.line);
                lex_next();
                char *lib = NULL;
                if (lex_cur.type == TOK_AT) {
                    lex_next();
                    if (lex_cur.type != TOK_ID) fatal("line %d: expected library name after '@'", lex_cur.line);
                    lib = sdup(lex_cur.val.str);
                    lex_next();
                }
                ASTNode *n = ast_alloc(NODE_METHOD_CALL);
                n->data.method_call.receiver = e;
                n->data.method_call.method = method;
                n->data.method_call.args = args;
                n->data.method_call.argc = nargs;
                n->data.method_call.lib = lib;
                e = n;
                continue;
            }
            ASTNode *f = ast_alloc(NODE_INDEX);
            f->data.idx.container = e;
            f->data.idx.index = ast_str(lex_cur.val.str);
            lex_next();
            e = f;
            continue;
        }
        lex_next();
        ASTNode *idx = parse_expr();
        if (lex_cur.type != TOK_RBRACKET) fatal("line %d: expected ']'", lex_cur.line);
        lex_next();
        ASTNode *n = ast_alloc(NODE_INDEX);
        n->data.idx.container = e;
        n->data.idx.index = idx;
        e = n;
    }
    return e;
}

ASTNode *parse_mul(void) {
    ASTNode *l = parse_unary();
    while (lex_cur.type == TOK_STAR || lex_cur.type == TOK_SLASH || lex_cur.type == TOK_MOD || lex_cur.type == TOK_INT_DIV) {
        int op = lex_cur.type;
        lex_next();
        ASTNode *r = parse_unary();
        l = ast_binop(op, l, r);
    }
    return l;
}

ASTNode *parse_add(void) {
    ASTNode *l = parse_mul();
    while (lex_cur.type == TOK_PLUS || lex_cur.type == TOK_MINUS) {
        int op = lex_cur.type;
        lex_next();
        ASTNode *r = parse_mul();
        l = ast_binop(op, l, r);
    }
    return l;
}

ASTNode *parse_cmp(void) {
    ASTNode *l = parse_add();
    while (lex_cur.type == TOK_LT || lex_cur.type == TOK_GT || lex_cur.type == TOK_LE ||
           lex_cur.type == TOK_GE || lex_cur.type == TOK_EQ || lex_cur.type == TOK_NE) {
        int op = lex_cur.type;
        lex_next();
        ASTNode *r = parse_add();
        l = ast_binop(op, l, r);
    }
    return l;
}

ASTNode *parse_and(void) {
    ASTNode *l = parse_cmp();
    while (lex_cur.type == TOK_AND || lex_cur.type == TOK_AND_AND) {
        int op = lex_cur.type;
        lex_next();
        ASTNode *r = parse_cmp();
        l = ast_binop(op, l, r);
    }
    return l;
}

ASTNode *parse_or(void) {
    ASTNode *l = parse_and();
    while (lex_cur.type == TOK_OR) {
        lex_next();
        ASTNode *r = parse_and();
        l = ast_binop(TOK_OR, l, r);
    }
    return l;
}

ASTNode *parse_expr(void) {
    return parse_or();
}

char *str_lower(const char *s) {
    size_t n = strlen(s);
    char *r = malloc(n + 1);
    if (!r) fatal("out of memory");
    for (size_t i = 0; i < n; i++) r[i] = tolower((unsigned char)s[i]);
    r[n] = 0;
    return r;
}

int is_word_bound(char c) {
    return !isalnum((unsigned char)c) && c != '_';
}

int check_cond_flags(ASTNode *cond, int flags) {
    if (cond->type != NODE_BINOP) {
        fatal("line %d: nocase/anywhere/word require a comparison", cond->line);
    }
    int op = cond->data.binop.op;
    if (op != TOK_EQ && op != TOK_NE) {
        fatal("line %d: nocase/anywhere/word only work with == or !=", cond->line);
    }

    Value lv = eval_expr(cond->data.binop.left);
    Value rv = eval_expr(cond->data.binop.right);
    char *ls = val_tostr(lv);
    char *rs = val_tostr(rv);
    int result = 0;

    if (flags & 4) {
        char *haystack = (flags & 1) ? str_lower(ls) : sdup(ls);
        char *needle = (flags & 1) ? str_lower(rs) : sdup(rs);
        size_t hl = strlen(haystack), nl = strlen(needle);
        result = 0;
        for (size_t i = 0; i + nl <= hl; i++) {
            if (memcmp(haystack + i, needle, nl) == 0) {
                int before = (i == 0) || is_word_bound(haystack[i-1]);
                int after = (i + nl >= hl) || is_word_bound(haystack[i+nl]);
                if (before && after) { result = 1; break; }
            }
        }
        free(haystack); free(needle);
    } else if (flags & 2) {
        char *haystack = (flags & 1) ? str_lower(ls) : ls;
        char *needle = (flags & 1) ? str_lower(rs) : rs;
        result = (strstr(haystack, needle) != NULL) ? 1 : 0;
        if (flags & 1) { free(haystack); free(needle); }
    } else if (flags & 1) {
        char *ll = str_lower(ls);
        char *lr = str_lower(rs);
        result = (strcmp(ll, lr) == 0) ? 1 : 0;
        free(ll); free(lr);
    }

    free(ls); free(rs);
    val_free(lv); val_free(rv);
    return (op == TOK_EQ) ? result : !result;
}

int check_has(ASTNode *lhs_expr, ASTNode *item, ASTNode **items, int nitems, int nocase) {
    Value lv = eval_expr(lhs_expr);
    char *ls = val_tostr(lv);
    int result = 0;

    if (nitems > 0) {
        result = 1;
        for (int i = 0; i < nitems; i++) {
            Value rv = eval_expr(items[i]);
            char *rs = val_tostr(rv);

            char *haystack = nocase ? str_lower(ls) : ls;
            char *needle = nocase ? str_lower(rs) : rs;
            int found = (strstr(haystack, needle) != NULL) ? 1 : 0;
            if (nocase) { free(haystack); free(needle); }

            if (!found) result = 0;
            free(rs); val_free(rv);
        }
    } else if (item) {
        Value rv = eval_expr(item);
        char *rs = val_tostr(rv);
        char *haystack = nocase ? str_lower(ls) : ls;
        char *needle = nocase ? str_lower(rs) : rs;
        result = (strstr(haystack, needle) != NULL) ? 1 : 0;
        if (nocase) { free(haystack); free(needle); }
        free(rs); val_free(rv);
    }

    free(ls);
    val_free(lv);
    return result;
}
