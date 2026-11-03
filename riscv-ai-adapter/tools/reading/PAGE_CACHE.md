# Current-page cache

The read-only reader caches text and headings in the actual slideshow order returned by XSlideShowController.getSlideByIndex. It still reads the current slide index and confirms navigation before publishing voice context. Hidden/custom shows must not be mapped by raw document index. A new controller, changed slide count, or document mode clears/rebuilds the cache.

The change avoids fetching a full UNO page object and its name on every refresh/navigation, and avoids restarting the same slide at the first/last boundary. It does not cache rendered camera frames or change the source presentation.

2026-11-02 validation: four cache identity/boundary checks plus fourteen existing reader-core checks passed. For the supplied 32-slide deck, four formal endpoint requests averaged 5.23 seconds before and 2.79 seconds after; one cached request still took 5.68 seconds. This is page/context confirmation time, not a guarantee of completed screen rendering or speech latency.

API: https://api.libreoffice.org/docs/idl/ref/interfacecom_1_1sun_1_1star_1_1presentation_1_1XSlideShowController.html
