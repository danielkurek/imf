# Výsledky měření

Ve složce `logs/` jsou umístěné log soubory zařízení z jednoho průběhu hry `finding-color`. Navíc jsou ve složce soubory `colors.csv`,`path_0011.csv`,`positions.csv`, které doplňují informace, které nejsou v log souborech.

- `colors.csv` - použité barvy ke stanicím
- `positions.csv` - srovnává reálné polohy s vypočítanými a polohami po korekci
- `path_0011.csv` reprezentuje část reálné cesty zařízení 0011 (cesta je srovnaná s vypočítanou cestou v souboru `plot-locations.ipynb`).

Python program `log_parser.py` zpracovává data z log souborů a ukládá je do `.json` souborů. Pro převod všech log souborů můžeme využít shell skript `parse_logs.sh`.

`plot-locations.ipynb` je Jupyter notebook (Python 3.12), který vytváří vizualizace dat ze zařízení. Jednou vizualizací je animace cesty jednoho mobilního zařízení, která je uložená v této složce (soubor `finding-color-mobile-path-0011.mp4`). K animaci byl vytvoření i komentář (`finding-color-mobile-path-0011-komentar.md`), který popisuje animace a diskutuje výsledky určení polohy.

V souboru `requirements.txt` jsou potřebné knihovny pro spuštění python programů v této složce.