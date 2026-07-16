# Scientific Sources

Use these sources when explaining design choices or interpreting results. Distinguish standards from research papers.

## Core L4S and Prague

- Briscoe, B., and De Schepper, K. “Resolving Tensions between Congestion Control Scaling Requirements.” 2019. arXiv:1904.07605. https://arxiv.org/abs/1904.07605
- Briscoe, B. “The Native AQM for L4S Traffic.” 2019. arXiv:1904.07079. https://arxiv.org/abs/1904.07079
- Briscoe, B., and Ahmed, A. S. “TCP Prague Fall-back on Detection of a Classic ECN AQM.” 2019. arXiv:1911.00710. https://arxiv.org/abs/1911.00710
- Sarpkaya, F. B., Fund, F., and Panwar, S. “To Adopt or Not to Adopt L4S-Compatible Congestion Control? Understanding Performance in a Partial L4S Deployment.” 2024. arXiv:2411.10952. https://arxiv.org/abs/2411.10952
- Alioua, N., Zhang, L., Garg, A., Yan, F. Y., and Belding, E. “A DualPI2 Module for Mahimahi: Behavioral Characterization and Cross-Platform Analysis.” 2026. arXiv:2603.04381. https://arxiv.org/abs/2603.04381

## ECN estimator background

- Alizadeh, M., Greenberg, A., Maltz, D. A., Padhye, J., Patel, P., Prabhakar, B., Sengupta, S., and Sridharan, M. “Data Center TCP.” ACM SIGCOMM 2010. https://doi.org/10.1145/1851182.1851192

## QUIC architecture and recovery

- Langley, A., Riddoch, A., Wilk, A., Vicente, A., Krasic, C., Zhang, D., Yang, F., Kouranov, F., Swett, I., Iyengar, J., Bailey, J., Dorfman, J., Roskind, J., Kulik, J., Westin, P., Tenneti, R., Shade, R., Hamilton, R., Vasiliev, V., Chang, W.-T., and Shi, Z. “The QUIC Transport Protocol: Design and Internet-Scale Deployment.” ACM SIGCOMM 2017. https://doi.org/10.1145/3098822.3098842
- Iyengar, J., and Thomson, M. “QUIC: A UDP-Based Multiplexed and Secure Transport.” RFC 9000, 2021. https://www.rfc-editor.org/rfc/rfc9000
- Iyengar, J., and Swett, I. “QUIC Loss Detection and Congestion Control.” RFC 9002, 2021. https://www.rfc-editor.org/rfc/rfc9002

## L4S standards

- Briscoe, B., De Schepper, K., Bagnulo, M., and White, G. “Low Latency, Low Loss, and Scalable Throughput (L4S) Internet Service: Architecture.” RFC 9330, 2023. https://www.rfc-editor.org/rfc/rfc9330
- De Schepper, K., and Briscoe, B. “The Explicit Congestion Notification (ECN) Protocol for Low Latency, Low Loss, and Scalable Throughput (L4S).” RFC 9331, 2023. https://www.rfc-editor.org/rfc/rfc9331
- De Schepper, K., Briscoe, B., and White, G. “DualQ Coupled Active Queue Management (AQM) for Low Latency, Low Loss, and Scalable Throughput (L4S).” RFC 9332, 2023. https://www.rfc-editor.org/rfc/rfc9332

## Citation practice

Cite the pinned picoquic and IMQUIC source commits separately from papers. Treat code behavior as an implementation fact and papers as scientific motivation or interpretation. Do not claim that a parameter setting is standard merely because it appears in one implementation.
