type PageLoadingSpinnerProps = {
  size?: number;
};

export function PageLoadingSpinner({ size = 28 }: PageLoadingSpinnerProps) {
  return (
    <div
      className="animate-spin"
      aria-hidden="true"
      style={{
        width: size,
        height: size,
        border: '2px solid #e0e0e0',
        borderTopColor: '#1890ff',
        borderRadius: '50%',
      }}
    />
  );
}

type PageLoadingStateProps = {
  size?: number;
  height?: number;
};

export function PageLoadingState({ size = 28, height = 200 }: PageLoadingStateProps) {
  return (
    <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'center', height }}>
      <PageLoadingSpinner size={size} />
    </div>
  );
}

export default PageLoadingSpinner;
