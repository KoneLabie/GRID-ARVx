"""Validate native FX transport and captures; no reconstructed algorithm oracle."""
import json
from pathlib import Path
import sys
import numpy as np
from PIL import Image

def run(folder):
    backend = json.loads((folder / "backend.json").read_text(encoding="utf-8"))
    if backend.get("pixelExpectations") != "native":
        raise ValueError("This suite requires the native-engine capture backend")
    checks, images = [], {}
    def check(passed, description):
        checks.append(("PASS " if passed else "FAIL ") + description)
    for line in (folder / "inventory.txt").read_text(encoding="utf-8").splitlines():
        filename, name = line.split("\t", 1)
        index = int(Path(filename).stem)
        state = json.loads((folder / f"{index}.native.json").read_text(encoding="utf-8"))
        image = Image.open(folder / f"{index}.native.png").convert("RGBA")
        values = np.asarray(image)
        images[name] = values[:, :, :3]
        check(image.size == (state["renderWidth"], state["renderHeight"]) and np.all(values[:, :, 3] == 255), name + " / complete opaque native framebuffer")
        check(np.std(values[:, :, :3]) > 2, name + " / visible native source survives capture")
        target, amount = None, 0
        if name.startswith(("noise-", "grain-")):
            target = "texture"
            amount = int(name.rsplit("-", 1)[1]) if "-amount-" in name else 90
        elif name.startswith("symbol-"):
            target = "dither"
            amount = int(name.rsplit("-", 1)[1]) if "-amount-" in name else 127
        elif name.startswith("glass-") and name != "glass-baseline":
            target = "glass"
            amount = int(name.rsplit("-", 1)[1]) if "-amount-" in name else 127
        expected = {key: (amount / 127 if key == target else 0) for key in ("texture", "dither", "track", "glass")}
        check(all(abs(state[key] - value) < .0001 for key, value in expected.items()), name + " / requested FX amount and bypass values reach native renderer")
    names = ["glass-ball-amount-127", "glass-2x2", "glass-Full", "glass-Flower", "glass-Cross", "glass-Burst"]
    # Original shapes must remain distinct. This intentionally does not impose
    # the retired renderer's altered outside-lens brightness or random model.
    distinct = min(float(np.mean(np.abs(images[a].astype(float) - images[b].astype(float))))
                   for i, a in enumerate(names) for b in names[i + 1:])
    check(distinct > 2, "Six native Glass profiles produce distinct captured output")
    text = "\n".join(checks) + "\n"
    (folder / "native-image-checks.txt").write_text(text, encoding="utf-8")
    print(f"Native image checks: {sum(line.startswith('PASS') for line in checks)} PASS, {sum(line.startswith('FAIL') for line in checks)} FAIL")
    print("Native algorithm fidelity and subjective appearance still require original-program reference comparison.")
    return int(any(line.startswith("FAIL") for line in checks))

if __name__ == "__main__":
    raise SystemExit(run(Path(sys.argv[1])))
