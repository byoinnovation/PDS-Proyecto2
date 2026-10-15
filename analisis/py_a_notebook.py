import pathlib, re
import nbformat as nbf

src = pathlib.Path(__file__).with_name("analisis_snr.py").read_text(encoding="utf-8")
nb = nbf.v4.new_notebook()
for bloque in re.split(r"^# %%", src, flags=re.M)[1:]:
    cabecera, _, cuerpo = bloque.partition("\n")
    if "[markdown]" in cabecera:
        texto = "\n".join(l[2:] if l.startswith("# ") else l.lstrip("#") for l in cuerpo.strip().splitlines())
        nb.cells.append(nbf.v4.new_markdown_cell(texto))
    else:
        nb.cells.append(nbf.v4.new_code_cell(cuerpo.strip("\n")))
nb.metadata["kernelspec"] = {"name": "python3", "display_name": "Python 3", "language": "python"}
nbf.write(nb, pathlib.Path(__file__).with_name("analisis_snr.ipynb"))
print("Notebook con", len(nb.cells), "celdas")
