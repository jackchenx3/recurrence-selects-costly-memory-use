# Structured literature and novelty review

Date: 2026-10-01. Scope: `PHASE2-MUTABLE-MEMORY-001`.

## Search method

This is a structured, reproducible search of open scholarly metadata, followed by targeted reading of primary papers and citation chaining. It is not a PRISMA systematic review and does not support a claim of exhaustive coverage.

Seven prespecified text queries were run against OpenAlex and Europe PMC for work published from 1990 onward:

1. `"evolution of memory" changing environments`
2. `"phenotypic memory" evolution fluctuating environment`
3. `memory exploration evolution changing environments`
4. `learning memory evolution cost environmental predictability`
5. `"memory use" digital organisms evolution`
6. `adaptation cost memory fluctuating environments`
7. `environmental memory allele selection`

The archived search contains 838 returned records and 786 DOI/title-deduplicated records. A mechanical title screen retained 202 records with at least one strong topic term. The search script, full API responses, query URLs, deduplicated records and title-screen table are preserved in this directory. Primary papers were included when they modeled or measured selection, fitness or performance of memory, learning, historical information, anticipatory response, or related plasticity under environmental change. Neuroscience papers about the mechanism of an already present memory system, unrelated uses of “memory,” reviews without directly relevant results, and generic optimization algorithms were excluded from the scientific comparison. Citation chaining from the closest included papers recovered older digital-organism and behavioral models.

## Closest prior results

The broad proposition that predictability or recurrence can favor memory is well established.

- Kuijper and Johnstone (2016), *Parental effects and the evolution of phenotypic memory*, modeled genetically determined parental transmission rules and found that environmental stability and predictability can favor phenotypic memory ([doi:10.1111/jeb.12778](https://doi.org/10.1111/jeb.12778)).
- Carja and Plotkin (2017), *The evolutionary advantage of heritable phenotypic heterogeneity*, studied fixation of a modifier that permits heritable phenotypic heterogeneity and showed that intermediate memory can maximize fixation probability in periodic environments ([doi:10.1038/s41598-017-05214-2](https://doi.org/10.1038/s41598-017-05214-2)).
- Wright et al. (2022), *A reaction norm framework for the evolution of learning*, let memory weighting and current sampling effort evolve under explicit costs; memory increased with temporal autocorrelation and interacted with costly sampling ([doi:10.1111/brv.12879](https://doi.org/10.1111/brv.12879)).
- Arehart and Adler (2023), *A minimal model of learning*, analyzed the cost and benefit of memory duration and exploration in changing environments ([doi:10.1098/rspb.2023.1084](https://doi.org/10.1098/rspb.2023.1084)).
- Morgan, Suchow and Griffiths (2020), *Experimental evolutionary simulations of learning, memory and life history*, used simulated genetic loci for learning and memory, both with fitness costs, and found that environmental unpredictability suppressed memory evolution ([doi:10.1098/rstb.2019.0504](https://doi.org/10.1098/rstb.2019.0504)). This is the closest precedent for an explicitly mutable, costly memory-use trait.
- Grabowski et al. (2010), *Early Evolution of Memory Usage in Digital Organisms*, evolved self-replicating programs and showed that memory use can emerge when past information materially aids a task. This directly occupies any broad claim that memory use first emerged in an artificial evolutionary system.
- Kronholm (2022) modeled costly anticipatory epigenetic effects and found that they evolve only over restricted ranges of environmental rate and predictability ([doi:10.1093/eep/dvac007](https://doi.org/10.1093/eep/dvac007)).
- Dey, Proulx and Teotónio (2016) showed that temporal environmental structure can select maternal effects and transgenerational historical information ([doi:10.1371/journal.pbio.1002388](https://doi.org/10.1371/journal.pbio.1002388)).
- Skanata and Kussell (2016) derived evolutionary regimes for response networks with phenotypic memory under random environmental fluctuations ([doi:10.1103/PhysRevLett.117.038104](https://doi.org/10.1103/PhysRevLett.117.038104)).

The value and costs of already present memory are also well established.

- Dunlap and Stephens (2012) linked environmental change to optimal sampling and memory weighting ([doi:10.1016/j.beproc.2011.10.005](https://doi.org/10.1016/j.beproc.2011.10.005)).
- Lambert and Kussell (2014) measured bacterial memory under fluctuating carbon sources and modeled its long-term fitness benefit ([doi:10.1371/journal.pgen.1004556](https://doi.org/10.1371/journal.pgen.1004556)).
- Rescan et al. (2020) linked phenotypic memory to population growth and extinction risk in noisy environments ([doi:10.1038/s41559-019-1089-6](https://doi.org/10.1038/s41559-019-1089-6)).
- George (2023) optimized memory-driven phenotypic switching in fluctuating environments ([doi:10.1016/j.bpj.2023.10.019](https://doi.org/10.1016/j.bpj.2023.10.019)); Jain, Jolly and George (2025) added adaptation costs ([doi:10.1101/2025.05.24.655868](https://doi.org/10.1101/2025.05.24.655868)).
- Pollack, Nozoe and Kussell (2025) experimentally analyzed selection on gene regulation and phenotypic memory under metabolic fluctuations ([doi:10.1101/2025.06.23.661119](https://doi.org/10.1101/2025.06.23.661119)).
- Leadbeater and Hollis (2025) reviewed the multiple biological dimensions on which memory can evolve, reinforcing that “memory” is not a single scalar trait ([doi:10.1098/rstb.2024.0109](https://doi.org/10.1098/rstb.2024.0109)).

## Novelty decision

The completed study does **not** justify claims that it is the first demonstration that memory evolves, that environmental predictability selects memory, or that memory can carry a cost. Those claims are occupied by the literature above.

The defensible contribution is the conjunction of four design elements and the resulting decoupling:

1. a mutation-generated binary allele controls whether an individual uses an already supplied private cache in a finite, explicitly stochastic population;
2. using memory replaces one of exactly two fresh proposals, creating a transparent one-for-one search-budget opportunity cost rather than an added scalar fitness penalty;
3. a matched SHAM arm supplies an exact neutral mutation-drift benchmark for the same allele dynamics; and
4. allele enrichment and population accuracy are prospectively judged as different outcomes.

Under the one aligned lag-two recurrence law, the allele was strongly enriched relative to both the neutral baseline and the nonrecurrent control, while population accuracy failed the prespecified one-correct-bit scale and its recurrence interaction remained unresolved. The paper's contribution is therefore a controlled example in which selection on a costly information-use policy and consequential population performance separate. The paper must present this as a model-specific result and a measurement design, not as a general theory of memory evolution.

## Publication decision

A focused computational paper remains warranted because the primary outcome was prospective, the implementation and cohorts are new, the neutral comparison is exact, and the selection/performance divergence is scientifically informative. The paper should foreground the divergence rather than the positive allele result alone. It should avoid priority language such as “first,” “novel mechanism,” or “memory emerges de novo.”

No new parameter scan or numerical variant is needed before drafting. The next useful work is a manuscript and reproducibility package with this literature boundary, followed by one independent claim audit.

## Limits of this review

OpenAlex and Europe PMC do not cover every book, conference proceeding or subscription-only index uniformly. Citation chaining was targeted rather than exhaustive, and no Web of Science or Scopus export was available. Consequently, this review supports a bounded distinct-contribution claim and conservative wording, not an exhaustive priority claim.
