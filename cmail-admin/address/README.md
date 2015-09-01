This tool gives control over the addressses cmail will accept and assign them to users.

Arguments
=========

* ``` --verbosity, -v <level> ``` Sets the verbosity level (0 - 4).
* ``` --dbpath, -d <dbpath> ```   Path to master database.
* ``` --help, -h ```              Shows the help.

Actions
=======

add
-------
Syntax: ``` add <expression> <router> [<router argument> [<order>]] ```

Create a new inbound path entry.

The address expression _< expression >_ can be a full address (like ``` test@test.com ```)
or an SQL string pattern (see https://www.sqlite.org/lang_expr.html#like).

Example: To assign all paths beginning with `postmaster`, create an entry with the expression ``` postmaster% ```.
Inbound paths are matched against all expressions, sorted by their order value descending. The entry
with the highest weight will be taken.

delete
------
Syntax: ``` delete <order> ```

Deletes an expression from database.

swap
------
Syntax: ``` swap <first> <second> ```

This switches the order of two expressions.

list
----
Syntax: ``` list [<expression>] ```

Lists all addresses in database. If an expression is provided it only shows matching addresses.
