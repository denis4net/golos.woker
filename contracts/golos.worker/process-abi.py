#!/usr/bin/env python3
import sys
import json
import logging
import re

if __name__ == "__main__":
    logging.basicConfig(level=logging.DEBUG)

    abi = json.load(sys.stdin)

    for action in abi["actions"]: # add app_domain argument to all actions
        for action_definition in abi["structs"]:
            if action_definition["name"] != action["name"]:
                continue

            action_definition["fields"].insert(0, {
                "name": "app_domain",
                "type": "name"
            }
            )

    for action_definition in abi["structs"]:
        for field in action_definition["fields"]:
            m = re.match(r"set_t<([a-z_]+)>", field["type"])
            if m:
                field["type"] = "%s[]" % m.group(1)

    for t in abi["types"]:
        if t["new_type_name"] == "symbol_name": # patch for eosjs
            t["type"] = "symbol_code"

    json.dump(abi, sys.stdout, indent=4, separators=(',', ': '))
