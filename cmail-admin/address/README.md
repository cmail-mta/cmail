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
Syntax: ``` add <expression> <username> [<order>] ```

This adds an address to the list.

Every address is assigned to an user (added with the [cmail-admin-user](../user/) tool).
The address expression _< expression >_ can be a full address (like ``` test@test.com ```)
or an SQL string pattern (see https://www.sqlite.org/lang_expr.html#like).

As example for a pattern you can assign all postmaster addresses with ``` postmaster@% ```.
To sort the matching of addresses you can provide an order to every expression.
The match with the highest order will get the incoming mail. Valid values are higher than zero and must be unique.

delete
------
Syntax: ``` delete <expression> ```

Deletes a given address expression from database. The expression has to match an expression in list (Wildcards will not be interpreted).

switch
------
Syntax: ``` switch <exp1> <exp2> ```

This switches the order of two expressions. This means _< exp1 >_ has then the order number of _< exp2 >_ and vice versa.

list
----
Syntax: ``` list [<expression>] ```

Lists all addresses in database. If an expression is provided it only shows matched addresses.
