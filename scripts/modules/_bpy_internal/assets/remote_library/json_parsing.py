# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""Wrapper around cattrs."""

__all__ = [
    "ValidatingParser",
    "APIModel",
]

import dataclasses
import json
from typing import Any, Type, TypeVar

import cattrs
import cattrs.preconf.json

from . import blender_asset_library_openapi as api_models

# There is no common base class for dataclasses, so this type variable will have to act as a stand-in.
APIModel = TypeVar("APIModel")


class ValidatingParser:
    """Wrapper around cattrs, caching the cattrs converter."""

    _converter: cattrs.preconf.json.JsonConverter

    def __init__(self) -> None:
        self._converter = cattrs.preconf.json.JsonConverter(omit_if_default=True)

        # Register a custom unstructure hook for the type of `CustomPropertyV1.value`.
        #
        # NOTE: this MUST register the 'final' type, and cannot use
        # `CustomProperties` as an alias for `dict[str, CustomProperty]`. It
        # won't be found. It also has to include None in the union for some
        # reason, even though that's not declared in `CustomPropertyV1.value`.
        #
        # Basically cattrs told me to register a structure hook for this
        # specific type, and so that's what I (Sybren) did.
        self._converter.register_structure_hook(
            api_models.CustomPropertiesV1 | list[Any] | float | int | str | bool,
            lambda value, _: value,
        )

    def parse_and_validate(self, model_class: Type[APIModel], json_payload: bytes | str) -> APIModel:
        """Parse & validate the JSON data, returning an instance of the given model class.

        :raises json.JSONDecodeError: if the payload is not formatted as JSON.
        :raises cattrs.errors.ClassValidationError: if the payload doesn't pass
            validation and can't be converted to the given model class.
        """

        json_doc = json.loads(json_payload)
        return self._converter.structure(json_doc, model_class)

    def dumps(self, model_instance: Any) -> str:
        """Convert the model instance to JSON, returning it as string."""
        assert dataclasses.is_dataclass(model_instance), f"{model_instance} is not a dataclass"
        return self._converter.dumps(model_instance, indent=2)
