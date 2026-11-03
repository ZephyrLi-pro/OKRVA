import { fireEvent, render, screen } from '@testing-library/react';
import { describe, expect, it, vi } from 'vitest';
import {
  ChannelBindHeader,
  ChannelErrorAlert,
  ChannelManualConfigForm,
  ChannelModeSelector,
} from './ChannelBindUi';

describe('ChannelBindUi', () => {
  it('renders the shared bind header and error alert', () => {
    render(
      <>
        <ChannelBindHeader
          icon="wechat.svg"
          alt="WeChat"
          title="微信"
          description="扫码连接微信"
        />
        <ChannelErrorAlert message="连接失败" />
      </>,
    );

    expect(screen.getByAltText('WeChat')).toHaveAttribute('width', '64');
    expect(screen.getByText('微信')).toBeInTheDocument();
    expect(screen.getByText('扫码连接微信')).toBeInTheDocument();
    expect(screen.getByText('连接失败')).toHaveStyle({ color: '#ef4444' });
    expect(screen.getByText('连接失败').parentElement).toHaveStyle({
      display: 'flex',
      padding: '8px 16px',
      marginBottom: '16px',
    });
  });

  it('renders ring and filled mode selectors from one component', () => {
    const onChange = vi.fn();
    const options = [
      { value: 'qrcode', label: '扫码连接' },
      { value: 'manual', label: '手动连接' },
    ];
    const { rerender } = render(
      <ChannelModeSelector value="qrcode" options={options} onChange={onChange} />,
    );

    fireEvent.click(screen.getByText('手动连接'));
    expect(onChange).toHaveBeenCalledWith('manual');
    expect(screen.getByText('扫码连接').closest('label')).toHaveStyle({ marginRight: '24px' });
    expect(screen.getByText('手动连接').closest('label')).toHaveStyle({ marginRight: '0px' });
    expect(screen.getByText('扫码连接')).toHaveStyle({ color: 'rgba(0, 0, 0, 0.85)' });
    expect(screen.getByText('手动连接')).toHaveStyle({ color: 'rgba(0, 0, 0, 0.85)' });

    rerender(
      <ChannelModeSelector
        value="manual"
        options={options}
        onChange={onChange}
        variant="filled"
      />,
    );

    expect(screen.getByText('手动连接')).toHaveStyle({ color: 'rgba(0, 0, 0, 0.85)' });
  });

  it('wires the shared manual config form fields and actions', () => {
    const onPrimaryChange = vi.fn();
    const onSecretChange = vi.fn();
    const onToggleSecret = vi.fn();
    const onCancel = vi.fn();
    const onSubmit = vi.fn();

    render(
      <ChannelManualConfigForm
        primary={{
          label: 'App ID',
          value: '',
          onChange: onPrimaryChange,
          placeholder: '请输入 App ID',
        }}
        secret={{
          label: 'App Secret',
          value: '',
          onChange: onSecretChange,
          placeholder: '请输入 App Secret',
        }}
        showSecret={false}
        onToggleSecret={onToggleSecret}
        cancelLabel="取消"
        submitLabel="连接"
        onCancel={onCancel}
        onSubmit={onSubmit}
        submitting={false}
        canSubmit
      />,
    );

    fireEvent.change(screen.getByPlaceholderText('请输入 App ID'), { target: { value: 'app-id' } });
    fireEvent.change(screen.getByPlaceholderText('请输入 App Secret'), { target: { value: 'secret' } });
    const [toggleSecretButton] = screen.getAllByRole('button');
    expect(toggleSecretButton).toBeDefined();
    fireEvent.click(toggleSecretButton as HTMLElement);
    fireEvent.click(screen.getByText('取消'));
    fireEvent.click(screen.getByText('连接'));

    expect(onPrimaryChange).toHaveBeenCalledWith('app-id');
    expect(onSecretChange).toHaveBeenCalledWith('secret');
    expect(onToggleSecret).toHaveBeenCalledTimes(1);
    expect(onCancel).toHaveBeenCalledTimes(1);
    expect(onSubmit).toHaveBeenCalledTimes(1);
    expect(screen.getByPlaceholderText('请输入 App ID')).toHaveStyle({
      width: '100%',
      paddingLeft: '12px',
      paddingRight: '12px',
    });
    expect(screen.getByPlaceholderText('请输入 App Secret')).toHaveStyle({
      paddingRight: '36px',
    });
    const cancelButton = screen.getByText('取消');
    expect(cancelButton).toHaveStyle({ marginRight: '12px', background: '#E5E5E5' });

    fireEvent.mouseEnter(cancelButton);
    expect(cancelButton).toHaveStyle({ background: '#D9D9D9' });

    fireEvent.mouseDown(cancelButton, { button: 0 });
    expect(cancelButton).toHaveStyle({ background: '#CCCCCC' });

    fireEvent.mouseUp(cancelButton, { button: 0 });
    expect(cancelButton).toHaveStyle({ background: '#D9D9D9' });

    fireEvent.mouseLeave(cancelButton);
    expect(cancelButton).toHaveStyle({ background: '#E5E5E5' });

    const submitButton = screen.getByText('连接');
    expect(submitButton).toHaveStyle({ background: '#3790FA' });

    fireEvent.mouseEnter(submitButton);
    expect(submitButton).toHaveStyle({ background: '#2F7DE1' });

    fireEvent.mouseDown(submitButton, { button: 0 });
    expect(submitButton).toHaveStyle({ background: '#2568C7' });

    fireEvent.mouseUp(submitButton, { button: 0 });
    expect(submitButton).toHaveStyle({ background: '#2F7DE1' });

    fireEvent.mouseLeave(submitButton);
    expect(submitButton).toHaveStyle({ background: '#3790FA' });
  });
});
