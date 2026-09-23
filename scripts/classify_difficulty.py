"""Classifies the difficulty of Rummikub puzzles with a random forest.

The input is the feature csv from ./rummikub <puzzles.in> --features <out.csv>.
Every puzzle gets a label from the time the solver needed for it:
  easy < 0.0001 s <= moderate < 0.1 s <= medium < 1 s <= hard
The model only gets the features that we can compute before solving the puzzle.
The hyperparameters come from a grid search and the accuracy from 10-fold
cross-validation.

Example:
  python scripts/classify_difficulty.py features.csv --out-dir results
"""

import argparse
import os

import matplotlib
matplotlib.use("Agg") #write the figures to files, we do not need a window
import matplotlib.pyplot as plt
import pandas as pd
from sklearn.ensemble import RandomForestClassifier
from sklearn.metrics import ConfusionMatrixDisplay, classification_report, confusion_matrix
from sklearn.model_selection import GridSearchCV, KFold, cross_val_predict, cross_val_score

LABELS = ["1", "2", "3", "4"]
LABEL_NAMES = ["Easy", "Moderate", "Medium", "Hard"]


def categorize_difficulty(row):
    if row["execution_time"] < 0.0001:
        return "1"
    if row["execution_time"] < 0.1:
        return "2"
    if row["execution_time"] < 1.0:
        return "3"
    return "4"


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("features_csv")
    parser.add_argument("--out-dir", default="results", help="directory for the figures (default: results)")
    args = parser.parse_args()
    os.makedirs(args.out_dir, exist_ok=True)

    df = pd.read_csv(args.features_csv)
    df["difficulty"] = df.apply(categorize_difficulty, axis=1)
    print(df["difficulty"].value_counts().sort_index())

    features = df.drop(columns=["execution_time", "difficulty"])
    target = df["difficulty"]

    clf = RandomForestClassifier(random_state=36, class_weight="balanced")
    param_grid = {
        "n_estimators": [50, 100, 150],
        "max_depth": [None, 10, 20],
        "min_samples_split": [2, 5, 10],
        "min_samples_leaf": [1, 2, 4],
    }
    grid_search = GridSearchCV(estimator=clf, param_grid=param_grid, cv=10, n_jobs=-1, verbose=2, scoring="accuracy")
    grid_search.fit(features, target)
    print(f"Best Parameters: {grid_search.best_params_}")
    print(f"Best Cross-Validation Accuracy: {grid_search.best_score_}")
    best_clf = grid_search.best_estimator_

    #cross-validation with the best model
    k = 10
    cv = KFold(n_splits=k, shuffle=True, random_state=36)
    scores = cross_val_score(best_clf, features, target, cv=cv)
    print(f"{k}-Fold Cross-Validation Accuracy Scores:", scores)
    print("Average Accuracy:", scores.mean())

    y_pred_cv = cross_val_predict(best_clf, features, target, cv=cv)
    cm = confusion_matrix(target, y_pred_cv, labels=LABELS)
    ConfusionMatrixDisplay(confusion_matrix=cm, display_labels=LABEL_NAMES).plot(cmap=plt.cm.Blues)
    plt.title("Confusion Matrix (10-Fold Cross-Validation)")
    plt.savefig(os.path.join(args.out_dir, "confusion_matrix_cv.png"), dpi=300)
    plt.close()

    print("\nClassification Report:")
    print(classification_report(target, y_pred_cv, labels=LABELS, target_names=LABEL_NAMES, zero_division=0))

    #feature importance of the model trained on the whole dataset
    best_clf.fit(features, target)
    importance_df = pd.DataFrame({
        "Feature": features.columns,
        "Importance": best_clf.feature_importances_,
    }).sort_values(by="Importance", ascending=False)

    plt.figure(figsize=(10, 6))
    plt.barh(importance_df["Feature"], importance_df["Importance"], color="skyblue")
    plt.xlabel("Importance")
    plt.ylabel("Feature")
    plt.title("Feature Importance")
    plt.gca().invert_yaxis() #most important feature on top
    plt.tight_layout()
    plt.savefig(os.path.join(args.out_dir, "feature_importance.png"), dpi=300)


if __name__ == "__main__":
    main()
