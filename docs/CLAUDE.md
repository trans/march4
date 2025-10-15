# March 

VERSION: α₄

# OVERVIEW

March is a progrmming language built from the ground up --
from machine code to an ultra-modern high-productivity development system.

FORTH is first and foremost inspiration. When in doubt, it never hurts to ask "What would Chuck do?".

A program is stored in an SQLite database. But also has a textual representation for working in a traditional manner.

Privimative words are machine code (we might start with assembly however -- but we can use anything that can spit our machine code.

Primative words are stored with an index key that is a SHA256 content-hash of the machine code. We can call this a CID.

Primative machine code is also index by architechure. Other architectures can be supported simply by writing the necessary primatives
in an architecures machine code.

User words are stored with an index key that is CID of the CIDs that encode it's definition.

Literals are encoded (serialized) and and stored with a CID as well.

I am running CahyOS Liunx on Intel Core Ultra 7. (Obviously our first target architecture.)

The core model is essentially FORTH. 

There is a data stack and a return stack.

A program is read from the database and "stiched" together, in much the sam way as FORTH reads from a token stream.

See DESIGN.md document for details.

Also see SYNTAX.md document.

