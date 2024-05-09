# Animace cesty zařízení 0011

V animaci můžeme vidět cestu mobilního zařízení s adresou 0011. Zařízení se pohybovalo mezi stanicemi podle pravidel hry (pohybovalo vždy ke stanici se stejnou barvou, jako bylo na zařízení). U každé stanice je popisek s barvou, kterou měli po celou dobu průběhu animace.

Vypočítaná cesta mobilního zařízení je zobrazená modrou tečkovanou čárou. Oranžová čára je pouze pro srovnání s alternativní metodou výpočtu polohy, a to Levenberg-Marquardt, tato metoda byla spuštěna na PC s naměřenými daty ze zařízení. Pro přehlednost je vykresleno vždy pouze 5 posledních pozic zařízení.

V průběhu animace se zobrazují na obrazovce texty typu `ffffff->ff0000`, který značí změnu barvy mobilního zařízení (v tomto případě z bílé na červenou). Na obrazovce se zobrazují vždy pouze 2 poslední popisky tohoto typu.

Na animaci můžeme vidět, že polohy zařízení se občas určí špatně. To je způsobené nepřesnostmi v měření vzdálenosti. V některých případech to je způsobené i zvolenou metodou pro výpočet poloh, tento případ je vysvětlený v textu práce v kapitole 5.