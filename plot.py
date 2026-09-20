import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.cm as cm
import numpy as np

plt.style.use('seaborn-v0_8-darkgrid')
plt.rcParams['font.size'] = 11

try:
    df = pd.read_csv('results.csv')
except FileNotFoundError:
    print("Ошибка: Файл 'results.csv' не найден.")
    exit(1)

fig, ax = plt.subplots(figsize=(16, 9))
colors = cm.tab20(np.linspace(0, 1, len(df.columns[1:])))

for idx, column in enumerate(df.columns[1:]):
    line_style = '--' if '_C' in column else '-'
    linewidth = 3 if any(x in column for x in ['PDQ', 'Parallel', 'Radix', 'Branchless']) else 1.5
    y_values = df[column] / 1_000_000
    ax.plot(df['Size'], y_values, marker='o', linewidth=linewidth,
            linestyle=line_style, color=colors[idx],
            label=column.replace('_', ' '))

ax.set_title('Абсолютный бенчмарк алгоритмов сортировки (C vs FASM vs OMP)', fontsize=18, fontweight='bold', pad=20)
ax.set_xlabel('Размер массива (N)', fontsize=14)
ax.set_ylabel('Время выполнения (Миллионы тактов процессора)', fontsize=14)
ax.set_xscale('log', base=2)
ax.set_xticks(df['Size'])
ax.set_xticklabels([str(int(x)) for x in df['Size']])
ax.legend(title='Алгоритм', bbox_to_anchor=(1.02, 1), loc='upper left', ncol=2, fontsize=10)

plt.tight_layout()

filename = 'ultimate_benchmark_chart.png'
plt.savefig(filename, dpi=300, bbox_inches='tight')
print(f"График 'Все-против-всех' сохранен в файл: {filename}")

try:
    plt.show()
except Exception:
    pass
