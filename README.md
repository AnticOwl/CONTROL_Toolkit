# CONTROL Toolkit v0.5c — Guide utilisateur et technique

Édition de CONTROL 1 (DX12) : toolkit ImGui intégré, hotsampling, post-process, lumières natives Northlight Spot/Point, Camera Path, IGCSDOF et raccourcis configurables.

> **Politique Lights : réutiliser les lumières, pas les supprimer.** Cette version ne comporte **aucune commande Delete Light**. Une lumière créée peut être sélectionnée, désactivée temporairement, déplacée et reconfigurée pendant la session. Une réutilisation persistante entre niveaux via presets n'est **pas implémentée ni validée**. Ne pas confondre avec les boutons « Delete Node » du Camera Path, qui agissent uniquement sur ses nœuds.

## Installation et démarrage

1. Décompresser le paquet dans un dossier.
2. Démarrer CONTROL en **DX12** (`Control_DX12.exe`) et entrer dans le jeu.
3. Exécuter `Inject-Control.cmd` (ou `ControlToolkitInjector.exe`). La DLL doit rester à côté de l'injecteur.
4. Afficher/masquer le Toolkit avec **F10** (touche reconfigurable).

Le paquet contient les binaires x64 précompilés, le script de lancement et les notes de version. N'injecter qu'une fois par session.

## Onglets

### Hotsampling
- L'onglet affiche uniquement les résolutions utiles : **Window res** (sortie) et **Internal res** (rendu interne).
- Saisir la taille de fenêtre souhaitée, puis **Apply**. **Restore original** revient à la taille initiale.
- Le ratio de rendu interne sélectionné dans CONTROL, y compris DLSS réduit et supersampling, est conservé lors du redimensionnement.
#### Résolution interne, DLSS et compatibilité SRWE — observations en jeu

Le **hotsampling intégré** redimensionne la fenêtre de CONTROL et déclenche `WM_EXITSIZEMOVE`. Il ne commande **pas directement** le réglage *Render Resolution* interne du jeu. Les trois notions doivent être distinguées :

- **Window / Current window client** : taille actuelle de la zone cliente Windows.
- **DX12 swapchain** : dimensions de la surface d'échange, qui peuvent temporairement différer de celles de la fenêtre après Apply ou Restore.
- **Render Resolution** : résolution interne de rendu sélectionnée/gérée par CONTROL, qui peut être différente des deux précédentes, notamment avec **NVIDIA DLSS** activé.

**Comportements observés lors des essais :**

- Avec DLSS désactivé et le réglage **Native**, un essai à **3400 × 1800** a montré dans ReShade un buffer de profondeur probable à **3400 × 1800**, avec **1433 draw calls**.
- Lors d'un autre essai, une fenêtre demandée à **3400 × 1800** était accompagnée d'un buffer de profondeur à **4250 × 2250** (ratio **125 %** sur chaque axe). Cela suggère une divergence de résolution interne, sans démontrer à lui seul la cause exacte.
- Avec DLSS actif, CONTROL pouvait afficher **1280 × 720** pour la fenêtre et **2560 × 1440** pour *Render Resolution* ; après Restore, le Toolkit a également signalé une fenêtre de **1280 × 720** tandis que le swapchain DX12 était encore à **1700 × 900**. La resynchronisation n'est donc pas systématiquement immédiate.

**ReShade Generic Depth** : le buffer qui comporte beaucoup de *draw calls* est un indicateur utile pour repérer le rendu principal, **pas une preuve absolue**. Certains buffers volumineux (par exemple **16384 × 4096**) peuvent être des atlas ou des ressources auxiliaires. Vérifier en parallèle les options d'affichage du jeu.

**Conseils pratiques :**

1. Pour des captures avec correspondance fenêtre/rendu, commencer par **DLSS désactivé**, **Render Resolution = Native**, puis appliquer la résolution souhaitée depuis le Toolkit.
2. Comparer la fenêtre, le swapchain et les buffers pertinents dans ReShade. Au retour, vérifier aussi les options *Display* de CONTROL ; si le moteur conserve un ancien réglage interne, le sélectionner à nouveau dans le jeu.
3. **SRWE peut également être utilisé**, au choix, pour redimensionner la fenêtre de CONTROL. Il reste une **alternative facultative**, pas une dépendance du Toolkit. SRWE ne garantit pas à lui seul que la *Render Resolution* interne ou le swapchain DX12 suivent le redimensionnement.
4. Éviter de multiplier simultanément les redimensionnements par SRWE et par le Toolkit : utiliser un outil à la fois, puis vérifier le rendu. Le comportement en DLSS n'est pas entièrement validé.

**Limite connue :** le hook préserve les dimensions calculées par le moteur au point d’écriture documenté ; il ne remplace pas le choix du Render Scale ou du mode DLSS. Vérifier le ratio observé après resize.

### Post Process
L'ancien onglet Bloom s'appelle **Post Process** et regroupe Bloom Scale X/Y, Intensity, Threshold, Lens Dirt, Scatter et les paramètres Global Exposure, Radial Blur, Vignette. Les contrôles sont liés aux paramètres déjà disponibles dans le Toolkit.

### Rendering
Conserve les options de rendu existantes du projet. Leur disponibilité dépend des adresses résolues dans la version du jeu.

### Lights — Spot et Point/Omni
- Choisir **Spot** ou **Point / Omni** puis cliquer sur **Create Light** : la lumière est créée à la caméra.
- Chaque entrée de **Spawned lights** affiche son **type** et peut être sélectionnée pour l'éditer.
- **Transform (XYZ / PYR)** est en **haut**, identique pour Spot et Point : `X Y Z` sur la première ligne et `Pitch Yaw Roll` sur la deuxième.
- XYZ agit sur la matrice native avec le mapping **X=m[9], Y=m[11], Z=m[10]** (correction v0.4t) ; l'édition est immédiate.
- Attention : les commandes PYR sont **incrémentales**, pas des angles absolus affichant l'orientation actuelle. Une saisie ne remplace donc pas un angle Euler mémorisé.
- **Place Selected at Current Camera** déplace la lumière sélectionnée à la position/orientation de la caméra Photo Mode. La v0.4r force l'actualisation native immédiatement, sans devoir toucher Intensity.
- **Enabled (master)** permet de couper/rétablir l'émission de la lumière sans prétendre détruire l'objet. Les corrections du toggle Point de v0.4q sont conservées.
- **Intensity** utilise le champ natif sans plafond arbitraire fixe dans l'interface (la précision et les bornes effectives restent celles du moteur).
- **Color** se règle dans le sélecteur RGB.

#### Réglages Spot
- Intensity, Color, Field of view, Area light radius, Aspect ratio, Directionality, Near/Far plane.
- Perspective / Stereographic, Bent Normals, Direct Lighting, Far Clipping.
- Volumetric : activation, Scatter multiplier, Volumetric effect multiplier, lobes/blend, qualité et forme.
- Shadows : Depth bias, Depth slope bias, filter, type (Off/PCF/VSM), taille 128 à 1024.

#### Réglages Point / Omni
- Intensity, Color, **Source radius (area)** : décrit la taille de la source, **pas** sa portée d'éclairage ; un rayon plus grand peut diminuer la luminosité perçue à proximité.
- **Range clip** : portée d'éclairage, observée comme fonctionnelle en jeu.
- **Max draw distance** : distance d'affichage ; les valeurs proches de zéro doivent être manipulées avec prudence, car des comportements de coupure ont été observés.
- **Decay** : décroissance / atténuation. Effet précis non encore caractérisé.
- **Diameter** : dimension de source avec effet sur la répartition lumineuse ; une petite valeur peut augmenter l'éclairement apparent.
- **Point scatter intensity multiplier** (0–100) : setter de scatter du Point, pas un équivalent complet du volumetric Spot.
- **Cast shadows** et **Bent Normals** lorsque leurs fonctions natives sont disponibles.
- Le réglage Color, le toggle OFF/ON et le mouvement immédiat ont été confirmés fonctionnels dans les essais utilisateur précédents.

#### Réutilisation des lumières (important)
Une lumière créée reste dans la liste de la session du Toolkit. Pour la **réutiliser**, sélectionner son entrée, activer Enabled si nécessaire, placer avec *Place Selected at Current Camera* ou modifier ses champs Transform, puis ajuster son éclairage. **Aucun Delete Selected / Delete All n'est demandé ni intégré.**

La survie de l'objet natif lors d'un changement de niveau est **inconnue** : il ne faut pas réutiliser un pointeur runtime après rechargement du niveau sans le revalider. Aucun enregistrement/chargement de presets de lumières n'existe dans cette v0.4u. Une fonctionnalité future de réutilisation inter-niveaux devra stocker les paramètres (type, transform, couleur, intensité, portée, volumetric, ombres...) puis recréer un objet valide dans la scène chargée.

### Camera Path
- Ajouter, mettre à jour, parcourir les nœuds de la caméra Photo Mode ; lecture Catmull-Rom pour les positions et interpolation des rotations, timings secondes/FPS et easing.
- Sauvegarder/charger `ControlToolkit_CameraPath.ctlpath`, enregistré à côté de `ControlToolkit.dll` ; le statut affiche le chemin complet après Save/Load. Les nœuds peuvent être effacés (ce ne sont **pas** des lights).
- La visualisation 3D de la spline n'est pas implémentée dans cette révision.

### IGCSDOF
- Fournisseur Northlight RAW via named pipes `IGCSDOF_ProviderToBridge_v1` et `IGCSDOF_BridgeToProvider_v1` ; la capture caméra est distincte de la détection Photo Mode.
- L'état Ready/Waiting dépend de la caméra effectivement capturée.

### Configuration
- **Photo Mode - 8 to 100** est actif par défaut (ajustable dans Configuration).
- Rebind de toutes les actions exposées, sauvegarde dans `ControlToolkit.ini`.
- Par défaut F10 Toolkit, F5 ajout nœud, F6 suppression nœud, F7/F8 précédent/suivant, F9 lecture, F11/F12 sauvegarder/charger. Les conflits sont signalés.

## Restauration et limites
- À l'entrée du Photo Mode, le Toolkit prend un snapshot des valeurs concernées. `Reset All` restaure celui-ci ; l'option *Auto restore when Photo Mode closes* est activée par défaut.
- Un snapshot et un reset des paramètres Photo Mode ne constituent **pas** un système de sauvegarde des lumières natives.
- Résolution 3D interne non vérifiée automatiquement, angles PYR relatifs, pas de presets Lights inter-niveaux, pas de destruction native : ces limites sont intentionnelles et documentées.

## Historique
Consulter `CHANGES_v0.4*.md` pour les évolutions détaillées. Les fichiers historiques décrivent l'état de leur version respective ; ce README décrit la v0.5c.

## v0.4x — Session ratio diagnostics

The toolkit captures a stable initial Window/Render ratio independently for X and Y; it shows the expected render dimensions after hotsampling. This build does **not** automatically correct the native render resolution, because a supported engine resize API has not yet been validated. The shared native vector is read-only.
