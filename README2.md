---
subtitle:   README 4 students
author:     Gaspard VARENNES
version:    0.2
---

> [!NOTE]
> This folder contains resources intended for the people who will be working on the module/project (i.e., the learners).
> These resources must be placed in the module instances and must be accessible to the learners.

## WMS

This folder contains a ready-to-use *Warehouse Management System* environment for developing and testing the firmware's network communication **without relying on the physical machine**.

### Usage

```bash
unzip WMS.zip
cd WMS
docker compose up
```

| Service        | URL                   | Identifiants        |
| -------------- | --------------------- | ------------------- |
| Dolibarr (WMS) | http://localhost      | admin / admin       |
| PHPMyAdmin     | http://localhost:8085 | epitech / epitech13 |

### Pre configured data

- **Entrepôts** (= the 3 physical outputs of the conveyor belt) :
  - A : Epitech Marseille
  - B : Epitech Lyon
  - C : Epitech Paris
- **Produit d'exemple** : reference `Eau` (Water bottle), warehouse A by default..
- **Token API du convoyeur** : `1ZtIVnP5bQ9QMK983qXoii1kd3c72dRQ` (Use it inside the header `DOLAPIKEY` of each request REST to Dolibarr).

## Views

This folder contains multiple views of the conveyor belts and its parts.
