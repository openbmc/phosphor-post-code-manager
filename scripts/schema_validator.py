#!/usr/bin/env python3
# Copyright 2021-present Facebook. All Rights Reserved.
import argparse
import json
import sys

import jsonschema.validators


def validator(schema_path):
    with open(schema_path) as f:
        schema = json.load(f)
    validator = jsonschema.Draft202012Validator(schema)
    return validator


def validate_json_conf(schema, js):
    v = validator(schema)
    try:
        with open(js) as f:
            data = json.load(f)
            v.validate(data)
        return True
    except jsonschema.exceptions.ValidationError as e:
        print("Validation failed", file=sys.stderr)
        print(e, file=sys.stderr)
        return False


def main():
    parser = argparse.ArgumentParser(
        description="JSON Schema Validator",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    parser.add_argument(
        "input",
        nargs="+",
        help="Input handler configuration",
    )
    parser.add_argument(
        "-s",
        "--schema",
        default=None,
        help="Schema to validate against",
    )
    args = parser.parse_args()
    if args.schema is None:
        print("FAILURE: Schema is required. See help")
        sys.exit(1)
    schema = args.schema
    files = args.input
    for f in files:
        if validate_json_conf(schema, f):
            print("SUCCESS: ", f)
        else:
            print("FAILURE: ", f, file=sys.stderr)
            sys.exit(1)


if __name__ == "__main__":
    main()
