This tool is used to configure the inbound path expressions (recipient mail addresses) cmail will 
allow as well as which router to apply to inbound mail.

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

Delete an address expression from database by its order value.

swap
------
Syntax: ``` swap <first> <second> ```

Swap the order/priority value of two expressions given their current values.

test
----
Syntax: ``` test <mailpath> ```

Test which expressions would match a given incoming mail path.

list
----
Syntax: ``` list [<expression>] ```

Lists all addresses in database. Optionally filter the output for addresses matching an expression.
