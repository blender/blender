# SPDX-FileCopyrightText: 2025 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""Wrapper around fastjsonschema and cattrs."""

from __future__ import annotations

__all__ = [
    "ValidatingParser",
    "APIModel",
    "JSONSchemaDict",
]

import dataclasses
import functools
import json
from pathlib import PurePosixPath
from typing import Callable, TypeAlias, Any, Type, TypeVar

import cattrs
import cattrs.preconf.json
import fastjsonschema


JSONSchemaDict: TypeAlias = dict[str, Any]
JSONDocument: TypeAlias = dict[str, Any]
JSONValidator: TypeAlias = Callable[[JSONDocument], None]

# There is no common base class for dataclasses, so this type variable will have to act as a stand-in.
APIModel = TypeVar("APIModel")


class ValidatingParser:
    """Wrapper around fastjsonschema and cattrs.

    This wrapper reuses/caches some instances, which heavily speeds up
    performance. Make sure to just create a ValidatingParser once, and reuse it.
    """

    _openapi_spec: JSONSchemaDict
    """The OpenAPI specification."""

    _ref_resolver: RefResolver

    _converter: cattrs.preconf.json.JsonConverter

    def __init__(self, openapi_spec: JSONSchemaDict) -> None:
        self._openapi_spec = openapi_spec
        self._ref_resolver = RefResolver(self._openapi_spec)
        self._converter = cattrs.preconf.json.JsonConverter(omit_if_default=True)

    def parse_and_validate(self, model_class: Type[APIModel], json_payload: bytes | str) -> APIModel:
        """Parse & validate the JSON data, returning an instance of the given model class.

        :raises json.JSONDecodeError: if the payload is not formatted as JSON.
        :raises fastjsonschema.JsonSchemaException: if the payload does not pass validation.
        :raises cattrs.errors.ClassValidationError: if the payload cannot be converted to the given model class.
        """

        json_doc = json.loads(json_payload)
        validator = self._create_validator(model_class.__name__)
        validator(json_doc)
        return self._converter.structure(json_doc, model_class)

    def dumps(self, model_instance: Any) -> str:
        """Convert the model instance to JSON, returning it as string."""
        assert dataclasses.is_dataclass(model_instance), f"{model_instance} is not a dataclass"
        return self._converter.dumps(model_instance, indent=2)

    @functools.lru_cache(maxsize=None)
    def _create_validator(self, model_name: str) -> JSONValidator:
        """Create a JSON schema validator for the given model name."""

        model_schema = self._openapi_spec["components"]["schemas"][model_name]
        resolved_schema = self._ref_resolver.resolve(model_schema)

        validator: JSONValidator
        validator = fastjsonschema.compile(resolved_schema)

        return validator


class RefResolver:
    _schema_root: JSONSchemaDict
    """The entire OpenAPI spec document."""

    _cache: dict[str, JSONSchemaDict | None]
    """Cache of $ref paths to the sub-document they point to.

    This is here not only as a cache, but also as a way to prevent infinite
    loops when there are circular references.
    """

    def __init__(self, schema_root: JSONSchemaDict) -> None:
        self._schema_root = schema_root
        self._cache = {}

    def resolve(self, model_schema: JSONSchemaDict) -> JSONSchemaDict:
        """Recursively resolve $ref keys in a JSON Schema.

        This is necessary because the fastjsonschema library doesn't resolve these by itself.
        """

        if isinstance(model_schema, dict):
            if "$ref" not in model_schema:
                return {k: self.resolve(v) for k, v in model_schema.items()}

            ref_path = model_schema["$ref"]
            if not ref_path.startswith("#/"):
                raise ValueError(
                    "Only internal $ref supported, got: {}".format(ref_path)
                )

            if ref_path in self._cache:
                resolved = self._cache[ref_path]
                if resolved is None:
                    raise ValueError(
                        "Circular $ref found while resolving {}".format(ref_path)
                    )
                return resolved

            # Walk through the root schema document to find the referenced item.
            parts = ref_path.lstrip("#/").split("/")
            referenced = self._schema_root
            for part in parts:
                referenced = referenced[part]

            self._cache[ref_path] = None  # For detecting circular references.
            resolved = self.resolve(referenced)
            self._cache[ref_path] = resolved
            return resolved

        if isinstance(model_schema, list):
            return [self.resolve(item) for item in model_schema]

        return model_schema
