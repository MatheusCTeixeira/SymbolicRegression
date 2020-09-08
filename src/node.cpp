#include "node.hpp"

#include <exception>
#include <iterator>
#include <algorithm>
#include <cmath>

using namespace std::string_literals;

bool gene_t::operator<(const gene_t& other) const
{
    return _value < other._value;
}

individual_t::individual_t(std::size_t depth)
    :
    _gen(pow(2, depth) - 1)
{
    clear();
}

individual_t::individual_t(const individual_t& other)
    :
    _gen{other._gen}
{
}

individual_t& individual_t::operator=(const individual_t& other)
{
    _gen = other._gen;

    return *this;
}

void individual_t::clear()
{
    clear_subtree(0);
}

void individual_t::clear_subtree(size_t node)
{
    _gen[node]._code = class_t::null;

    if (!has_child(node)) return;
    
    clear_subtree(lchild(node));
    clear_subtree(rchild(node));
}

void individual_t::clear_lsubtree(size_t node)
{
    if (has_child(node))
        return clear_subtree(lchild(node));
}

void individual_t::clear_rsubtree(size_t node)
{
    if (has_child(node))
        return clear_subtree(rchild(node));
}

std::size_t individual_t::depth() const
{
    auto p = std::find_if(_gen.rbegin(), _gen.rend(), [](gene_t g) {
        return g._code != class_t::null; });

    return std::log(std::distance(_gen.rend(), p) + 1);
}

bool individual_t::has_child(std::size_t node) const
{
    return rchild(node) < _gen.size();
}

bool individual_t::has_parent(std::size_t node) const
{
    return node > 0;
}

std::size_t individual_t::lchild(std::size_t parent) const
{
    return 2 * parent + 1;
}

std::size_t individual_t::rchild(std::size_t parent) const
{
    return 2 * parent + 2;
}

std::size_t individual_t::parent(std::size_t child) const
{
    if (!has_parent(child))
        throw std::invalid_argument{"individual_t::parent"s};

    return std::ceil((child - 1) / 2);
}

individual_handler_t::individual_handler_t(const random_t& rd)
    :
    _rd{rd}
{
}

void individual_handler_t::add_operator(std::string repr,
                                        std::size_t precedence,
                                        pOper oper)
{
    gene_t key {class_t::oper, (unsigned char)_oper.size()};
    auto value = make_tuple(repr, precedence, oper);

    _oper.insert({key, value});
}

void individual_handler_t::add_function(std::string repr,
                                        pFunc func)
{
    gene_t key {class_t::func, (unsigned char)_func.size()};
    auto value = make_tuple(repr, func);

    _func.insert({key, value});
}

void individual_handler_t::add_variable(std::size_t n_vars)
{
    _n_vars = n_vars;
}

gene_t individual_handler_t::rd_func() const
{
    auto chosen = _rd.func(0, _func.size());
    typename decltype(_func)::const_iterator b = _func.cbegin();
    std::advance(b, chosen);
    return b->first;
}

gene_t individual_handler_t::rd_oper() const
{
    auto chosen = _rd.oper(0, _oper.size());
    typename decltype(_oper)::const_iterator b = _oper.cbegin();
    std::advance(b, chosen);
    return b->first;
}

gene_t individual_handler_t::rd_var() const
{
    auto var = (unsigned char)_rd.var(0, _n_vars);
    gene_t g{class_t::var, var};
    return g;
}

gene_t individual_handler_t::rd_cons() const
{
    auto cons = (unsigned char)_rd.integer(0, 255);
    gene_t g{class_t::cons, cons};
    return g;
}
    
double individual_handler_t::eval(const individual_t& ind,
                                  vars_t vars) const
{
    return eval(ind, vars, 0);
}

std::string individual_handler_t::str(const individual_t& ind) const
{
    return str(ind, 0);
}

double individual_handler_t::eval(const individual_t& ind,
                                  vars_t vars,
                                  std::size_t n) const
{
    gene_t g = ind._gen.at(n);
    double result = 0;

    switch (g._code) {
    case class_t::oper:
        {
        auto lc = ind.lchild(n), rc = ind.rchild(n);
        auto lop = eval(ind, vars, lc), rop = eval(ind, vars, rc);
        result = eval_oper(g, lop, rop);
        }
        break;
        
    case class_t::func:
        {auto ind_var = ind.lchild(n);
        auto x = eval(ind, vars, ind_var);
        result = eval_func(g, x);} break;
        
    case class_t::var:
        {std::size_t i = g._value;
            result = vars[i];} break;
    case class_t::cons:
        {result = eval_cons(g);} break;
    default:
        throw std::invalid_argument{"individual_t::eval"};
    }

    return result;        
}

double individual_handler_t::eval_oper(const gene_t& g,
                                       double x,
                                       double y) const
{
    return std::get<2>(_oper.at(g))(x, y);
}

double individual_handler_t::eval_func(const gene_t& g,
                                       double x) const
{
    return std::get<1>(_func.at(g))(x);
}

double individual_handler_t::eval_cons(const gene_t& g) const
{
    unsigned char v = g._value;
    return -10 + 20 * v / 256;
}

std::string individual_handler_t::str(const individual_t& ind,
                                      std::size_t n) const
{
    std::string output{""};
    gene_t g = ind._gen.at(n);
    class_t code = g._code;
    
    switch (code) {
    case class_t::oper: {
        bool parentheses = need_parentheses(ind, n);
        std::size_t lc = ind.lchild(n), rc = ind.rchild(n);
        
        if (parentheses) output += "("s;
        output += str(ind, lc) + str_oper(g) + str(ind, rc);        
        if (parentheses) output += ")"s;}
        break;
    case class_t::func:
        output += str_func(g);
        output += "("s + str(ind, ind.lchild(n)) + ")"s; break;
    case class_t::var:
        output += str_var(g); break;
    case class_t::cons:
        output += str_cons(g); break;
    default:
        throw std::invalid_argument{"individual_t::str"};
    }

    return output;
}

bool individual_handler_t::need_parentheses(const individual_t& ind,
                                            std::size_t n) const
{
    if (!ind.has_parent(n)) return false;    
    if (ind._gen.at(ind.parent(n))._code != class_t::oper) return false;

    auto parent_pred = std::get<1>(_oper.at(ind._gen.at(ind.parent(n))));
    auto cur_pred = std::get<1>(_oper.at(ind._gen.at(n)));
    return cur_pred <= parent_pred;
}

std::string individual_handler_t::str_oper(const gene_t& g) const
{
    return std::get<0>(_oper.at(g));
}

std::string individual_handler_t::str_func(const gene_t& g) const
{
    return std::get<0>(_func.at(g));
}

std::string individual_handler_t::str_cons(const gene_t& g) const
{
    return std::to_string(eval_cons(g));
}

std::string individual_handler_t::str_var(const gene_t& g) const
{
    int subs = g._value;
    return "x_"s + std::to_string(subs);
}


#include <iostream>

using namespace std;

int main()
{
    individual_t a{7}, b{7};
    random_t rd{{1, 2, 3}};
    individual_handler_t hd{rd};
    hd.add_function("sin"s, [](double x) -> double { return sin(x); });
    hd.add_function("cos"s, [](double x) -> double { return cos(x); });
    hd.add_function("tan"s, [](double x) -> double { return tan(x); });
    hd.add_function("sinh"s, [](double x) -> double { return sinh(x); });
    hd.add_function("cosh"s, [](double x) -> double { return cosh(x); });
    hd.add_function("tanh"s, [](double x) -> double { return tanh(x); });
    hd.add_function("log"s, [](double x) -> double { return tanh(x); });
    hd.add_function("ln"s, [](double x) -> double { return tanh(x); });
    hd.add_function("log10"s, [](double x) -> double { return tanh(x); });
    hd.add_operator("+", 1, [](double x, double y) -> double { return x+y; });
    hd.add_operator("-", 2, [](double x, double y) -> double { return x-y; });
    hd.add_operator("/", 4, [](double x, double y) -> double { return x/y; });
    hd.add_operator("*", 3, [](double x, double y) -> double { return x*y; });
    hd.add_operator("^", 5, [](double x, double y) -> double { return pow(x, y); });
    hd.add_variable(1);
    a._gen[0] = hd.rd_oper();
    a._gen[1] = hd.rd_func();
    a._gen[2] = hd.rd_var();
    a._gen[3] = hd.rd_func();
    a._gen[7] = hd.rd_cons();
    cout << hd.str(a) << endl;
    return 0;
}
