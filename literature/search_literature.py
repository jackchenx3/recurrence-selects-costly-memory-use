#!/usr/bin/env python3
"""Archive a reproducible metadata search for the Phase-2 novelty review.

This script queries two open scholarly metadata services, stores their complete
responses, and builds a DOI/title-deduplicated candidate table. It does not
make inclusion or novelty decisions.
"""

import json
import re
import time
import urllib.parse
import urllib.request
from pathlib import Path


HERE = Path(__file__).resolve().parent
RAW = HERE / "raw"
RAW.mkdir(parents=True, exist_ok=True)

QUERIES = [
    '"evolution of memory" changing environments',
    '"phenotypic memory" evolution fluctuating environment',
    'memory exploration evolution changing environments',
    'learning memory evolution cost environmental predictability',
    '"memory use" digital organisms evolution',
    'adaptation cost memory fluctuating environments',
    'environmental memory allele selection',
]

UA = "Phase2 mutable-memory literature audit (mailto:chenx3@nih.gov)"


def fetch(url):
    request = urllib.request.Request(url, headers={"User-Agent": UA})
    with urllib.request.urlopen(request, timeout=60) as response:
        return json.load(response)


def norm_title(value):
    return re.sub(r"[^a-z0-9]+", " ", (value or "").lower()).strip()


records = []
query_log = []
for q_index, query in enumerate(QUERIES, 1):
    oa_url = (
        "https://api.openalex.org/works?search="
        + urllib.parse.quote(query)
        + "&filter=from_publication_date:1990-01-01,type:article"
        + "&sort=relevance_score:desc&per-page=50&mailto=chenx3@nih.gov"
    )
    oa = fetch(oa_url)
    (RAW / f"openalex_{q_index:02d}.json").write_text(
        json.dumps(oa, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    query_log.append({"service": "OpenAlex", "query": query, "url": oa_url})
    for rank, item in enumerate(oa.get("results", []), 1):
        abstract_index = item.get("abstract_inverted_index") or {}
        abstract_words = []
        if abstract_index:
            positions = []
            for word, indices in abstract_index.items():
                for pos in indices:
                    positions.append((pos, word))
            abstract_words = [word for _, word in sorted(positions)]
        records.append(
            {
                "service": "OpenAlex",
                "query": query,
                "rank": rank,
                "title": item.get("display_name"),
                "doi": (item.get("doi") or "").replace("https://doi.org/", "").lower(),
                "year": item.get("publication_year"),
                "url": item.get("doi") or item.get("id"),
                "abstract": " ".join(abstract_words),
            }
        )
    time.sleep(0.25)

    epmc_url = (
        "https://www.ebi.ac.uk/europepmc/webservices/rest/search?query="
        + urllib.parse.quote(query)
        + "&format=json&pageSize=100"
    )
    epmc = fetch(epmc_url)
    (RAW / f"europepmc_{q_index:02d}.json").write_text(
        json.dumps(epmc, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    query_log.append({"service": "Europe PMC", "query": query, "url": epmc_url})
    for rank, item in enumerate(epmc.get("resultList", {}).get("result", []), 1):
        records.append(
            {
                "service": "Europe PMC",
                "query": query,
                "rank": rank,
                "title": item.get("title"),
                "doi": (item.get("doi") or "").lower(),
                "year": item.get("pubYear"),
                "url": (
                    "https://doi.org/" + item["doi"]
                    if item.get("doi")
                    else "https://europepmc.org/article/MED/" + item.get("id", "")
                ),
                "abstract": item.get("authorString", ""),
            }
        )
    time.sleep(0.25)

(HERE / "query_log.json").write_text(
    json.dumps(query_log, indent=2, sort_keys=True) + "\n", encoding="utf-8"
)
(HERE / "records.json").write_text(
    json.dumps(records, indent=2, sort_keys=True) + "\n", encoding="utf-8"
)

dedup = {}
for record in records:
    key = "doi:" + record["doi"] if record["doi"] else "title:" + norm_title(record["title"])
    if key not in dedup:
        dedup[key] = {
            "title": record["title"],
            "doi": record["doi"],
            "year": record["year"],
            "url": record["url"],
            "appearances": [],
        }
    dedup[key]["appearances"].append(
        {"service": record["service"], "query": record["query"], "rank": record["rank"]}
    )

terms = {
    "memory": 3,
    "evolution": 3,
    "evolv": 3,
    "fluctuat": 2,
    "changing environment": 2,
    "predictab": 2,
    "cost": 1,
    "explor": 1,
    "allele": 1,
    "selection": 1,
    "phenotyp": 1,
}
for item in dedup.values():
    text = norm_title(item["title"])
    item["screen_score"] = sum(weight for term, weight in terms.items() if term in text)
    item["best_rank"] = min(x["rank"] for x in item["appearances"])

candidates = sorted(
    dedup.values(), key=lambda x: (-x["screen_score"], x["best_rank"], x["title"] or "")
)
(HERE / "candidates.json").write_text(
    json.dumps(candidates, indent=2, sort_keys=True) + "\n", encoding="utf-8"
)

lines = ["title\tyear\tdoi\tscore\tbest_rank\turl"]
for item in candidates:
    if item["screen_score"] < 3:
        continue
    lines.append(
        "\t".join(
            str(x or "")
            for x in (
                item["title"], item["year"], item["doi"], item["screen_score"],
                item["best_rank"], item["url"],
            )
        )
    )
(HERE / "candidate_screen.tsv").write_text("\n".join(lines) + "\n", encoding="utf-8")

summary = {
    "queries": len(QUERIES),
    "services": ["OpenAlex", "Europe PMC"],
    "records": len(records),
    "deduplicated_candidates": len(candidates),
    "title_screen_candidates_score_ge_3": len(lines) - 1,
}
(HERE / "SEARCH_RECEIPT.json").write_text(
    json.dumps(summary, indent=2, sort_keys=True) + "\n", encoding="utf-8"
)
print(json.dumps(summary, sort_keys=True))
