from pathlib import Path


def postprocess(generated_file_path: Path) -> None:
    """Post-process the file generated from blender_asset_library_openapi.yaml.

    This function is discovered and called by `make_generate_datamodels.py` when
    generating the datamodels.
    """
    contents = generated_file_path.read_text('utf-8')

    # Work around a limitation of datamodel-code-generator, see:
    # https://github.com/koxudaxi/datamodel-code-generator/issues/836
    #
    # This search & replace works, whereas the AST-based workaround mentioned
    # in the comments of that issue does not work for our case (it does quote
    # certain types, but not the one we need to be quoted).
    contents = contents.replace(
        "CustomPropertiesV1 = Optional[dict[str, CustomPropertyV1]]",
        "CustomPropertiesV1 = Optional[dict[str, 'CustomPropertyV1']]",
    )

    generated_file_path.write_text(contents, 'utf-8')
